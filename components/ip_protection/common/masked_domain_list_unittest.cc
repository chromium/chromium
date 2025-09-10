// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/masked_domain_list.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace ip_protection {

class MaskedDomainListTest : public testing::Test {
 public:
  base::FilePath MakeTempPath() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(scoped_temp_dir_.IsValid() ||
                scoped_temp_dir_.CreateUniqueTempDir());
    return scoped_temp_dir_.GetPath().AppendASCII(
        base::NumberToString(filename_suffix_++));
  }

  MaskedDomainList OpenMdl(base::FilePath& path) {
    base::File file(
        path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
    uint64_t size = file.GetLength();
    EXPECT_NE(size, 0UL);
    return MaskedDomainList(std::move(file), size);
  }

 private:
  int filename_suffix_ = 0;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(MaskedDomainListTest, Invalid) {
  base::FilePath path = MakeTempPath();

  MaskedDomainList mdl(base::File(), 10UL);
  EXPECT_EQ(mdl.Verify(), false);
}

TEST_F(MaskedDomainListTest, AddOwnerCollision) {
  base::FilePath path = MakeTempPath();
  auto bldr = MaskedDomainList::Builder();
  EXPECT_TRUE(bldr.AddOwner("example.com", 20, /*is_resource=*/true,
                            /*is_wildcard=*/false));
  EXPECT_FALSE(bldr.AddOwner("example.com", 30, /*is_resource=*/false,
                             /*is_wildcard=*/true));
  CHECK(bldr.Finish(path));

  MaskedDomainList mdl = OpenMdl(path);

  // Result matches the first call to AddOwner.
  EXPECT_EQ(mdl.Get("example.com"),
            (MaskedDomainList::GetResult{.owner_id = 20, .is_resource = true}));
}

TEST_F(MaskedDomainListTest, GetSubdomainHandling) {
  base::FilePath path = MakeTempPath();
  auto bldr = MaskedDomainList::Builder();
  EXPECT_TRUE(bldr.AddOwner("sub.foo.example.com", 20, /*is_resource=*/true,
                            /*is_wildcard=*/false));
  EXPECT_TRUE(bldr.AddOwner("foo.example.com", 10, /*is_resource=*/false,
                            /*is_wildcard=*/false));
  EXPECT_TRUE(bldr.AddOwner("example.com", 999, /*is_resource=*/true,
                            /*is_wildcard=*/true));
  CHECK(bldr.Finish(path));

  MaskedDomainList mdl = OpenMdl(path);
  EXPECT_EQ(
      mdl.Get("sub.foo.example.com"),
      (MaskedDomainList::GetResult{.owner_id = 20U, .is_resource = true}));
  EXPECT_EQ(
      mdl.Get("boat.foo.example.com"),
      (MaskedDomainList::GetResult{.owner_id = 999U, .is_resource = true}));
  EXPECT_EQ(
      mdl.Get("foo.example.com"),
      (MaskedDomainList::GetResult{.owner_id = 10U, .is_resource = false}));
  EXPECT_EQ(
      mdl.Get("bar.example.com"),
      (MaskedDomainList::GetResult{.owner_id = 999U, .is_resource = true}));
  EXPECT_EQ(
      mdl.Get("example.com"),
      (MaskedDomainList::GetResult{.owner_id = 999U, .is_resource = true}));
  EXPECT_EQ(mdl.Get("com"), (MaskedDomainList::GetResult{
                                .owner_id = 0U, .is_resource = false}));
  EXPECT_EQ(
      mdl.Get("example.co.uk"),
      (MaskedDomainList::GetResult{.owner_id = 0U, .is_resource = false}));
}

TEST_F(MaskedDomainListTest, IsOwnedResource) {
  base::FilePath path = MakeTempPath();
  auto bldr = MaskedDomainList::Builder();
  const uint32_t ACME = 10;
  EXPECT_TRUE(bldr.AddOwner("acme.com", ACME, /*is_resource=*/false,
                            /*is_wildcard=*/true));
  EXPECT_TRUE(bldr.AddOwner("acme.co.br", ACME, /*is_resource=*/false,
                            /*is_wildcard=*/true));
  EXPECT_TRUE(bldr.AddOwner("acme.net", ACME, /*is_resource=*/false,
                            /*is_wildcard=*/true));
  EXPECT_TRUE(bldr.AddOwner("acme-cdn.net", ACME, /*is_resource=*/true,
                            /*is_wildcard=*/true));
  const uint32_t GLOBEX = 20;
  EXPECT_TRUE(bldr.AddOwner("globex.com", GLOBEX, /*is_resource=*/false,
                            /*is_wildcard=*/false));
  EXPECT_TRUE(bldr.AddOwner("globex-cdn.net", GLOBEX, /*is_resource=*/true,
                            /*is_wildcard=*/true));
  CHECK(bldr.Finish(path));

  MaskedDomainList mdl = OpenMdl(path);

  EXPECT_FALSE(mdl.IsOwnedResource("initech.com"))
      << "Un-owned domain should not match";
  EXPECT_FALSE(mdl.IsOwnedResource("foo.initech.com"))
      << "Un-owned subdomain should not match";
  EXPECT_FALSE(mdl.IsOwnedResource("acme.com")) << "Property should not match";
  EXPECT_FALSE(mdl.IsOwnedResource("foo.acme.com"))
      << "Property subdomain should not match";
  EXPECT_FALSE(mdl.IsOwnedResource("acme.co.br"))
      << "Property should not match";
  EXPECT_FALSE(mdl.IsOwnedResource("foo.acme.co.br"))
      << "Property subdomain should not match";
  EXPECT_FALSE(mdl.IsOwnedResource("acme.net")) << "Property should not match";
  EXPECT_FALSE(mdl.IsOwnedResource("foo.acme.net"))
      << "Property subdomain should not match";
  EXPECT_TRUE(mdl.IsOwnedResource("acme-cdn.net")) << "Resource should match";
  EXPECT_TRUE(mdl.IsOwnedResource("foo.acme-cdn.net"))
      << "Resource subdomain should match";
}

TEST_F(MaskedDomainListTest, Matches) {
  base::FilePath path = MakeTempPath();
  auto bldr = MaskedDomainList::Builder();
  const uint32_t ACME = 10;
  EXPECT_TRUE(bldr.AddOwner("acme.com", ACME, /*is_resource=*/false,
                            /*is_wildcard=*/true));
  EXPECT_TRUE(bldr.AddOwner("acme.co.br", ACME, /*is_resource=*/false,
                            /*is_wildcard=*/true));
  EXPECT_TRUE(bldr.AddOwner("acme.net", ACME, /*is_resource=*/false,
                            /*is_wildcard=*/true));
  EXPECT_TRUE(bldr.AddOwner("acme-cdn.net", ACME, /*is_resource=*/true,
                            /*is_wildcard=*/true));
  const uint32_t GLOBEX = 20;
  EXPECT_TRUE(bldr.AddOwner("globex.com", GLOBEX, /*is_resource=*/false,
                            /*is_wildcard=*/false));
  EXPECT_TRUE(bldr.AddOwner("globex-cdn.net", GLOBEX, /*is_resource=*/true,
                            /*is_wildcard=*/true));
  CHECK(bldr.Finish(path));

  MaskedDomainList mdl = OpenMdl(path);

  // 1P requests.
  EXPECT_FALSE(mdl.Matches("initech.com", "initech.com"))
      << "1P request not on MDL should not match";
  EXPECT_FALSE(mdl.Matches("acme.com", "acme.co.br"))
      << "1P request to Acme property should not match";
  EXPECT_FALSE(mdl.Matches("acme-cdn.net", "acme.co.br"))
      << "1P request to Acme resource should not match";
  EXPECT_FALSE(mdl.Matches("pixel.acme-cdn.net", "acme.co.br"))
      << "1P request to Acme resource subdomain should not match";
  EXPECT_FALSE(mdl.Matches("acme-cdn.net", "www.acme.co.br"))
      << "1P request from subdomain to Acme resource should not match";
  EXPECT_FALSE(mdl.Matches("pixel.acme-cdn.net", "www.acme.co.br"))
      << "1P request from subdomain to Acme resource subdomain should not "
         "match";

  // 3P requests.
  EXPECT_FALSE(mdl.Matches("acme.com", "initech.com"))
      << "3P request to Acme property should not match";
  EXPECT_FALSE(mdl.Matches("www.acme.com", "initech.com"))
      << "3P request to Acme property subdomain should not match";
  EXPECT_TRUE(mdl.Matches("acme-cdn.net", "initech.com"))
      << "3P request to Acme resource should match";
  EXPECT_TRUE(mdl.Matches("pixel.acme-cdn.net", "initech.com"))
      << "3P request to Acme resource subdomain should match";
  EXPECT_TRUE(mdl.Matches("acme-cdn.net", "globex.com"))
      << "3P request from a listed owner property to Acme resource should "
         "match";
  EXPECT_TRUE(mdl.Matches("pixel.acme-cdn.net", "globex.com"))
      << "3P request from a listed owner property to Acme resource subdomain "
         "should match";
  EXPECT_TRUE(mdl.Matches("acme-cdn.net", "globex-cdn.net"))
      << "3P request from a listed owner resource to Acme resource should "
         "match";
  EXPECT_TRUE(mdl.Matches("pixel.acme-cdn.net", "globex-cdn.net"))
      << "3P request from a listed owner resource to Acme resource subdomain "
         "should match";
}

}  // namespace ip_protection

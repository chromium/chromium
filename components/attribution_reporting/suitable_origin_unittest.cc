// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/suitable_origin.h"

#include <optional>

#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {
namespace {

TEST(SuitableOriginTest, Create) {
  const struct {
    url::Origin origin;
    bool expected_suitable;
  } kTestCases[] = {
      {
          url::Origin(),
          false,
      },
      {
          url::Origin::Create(GURL("http://a.test")),
          false,
      },
      {
          url::Origin::Create(GURL("https://a.test")),
          true,
      },
      {
          url::Origin::Create(GURL("http://localhost")),
          true,
      },
      {
          url::Origin::Create(GURL("ws://a.test")),
          false,
      },
      {
          url::Origin::Create(GURL("wss://a.test")),
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_suitable,
              SuitableOrigin::IsSuitable(test_case.origin))
        << test_case.origin;

    std::optional<SuitableOrigin> actual =
        SuitableOrigin::Create(test_case.origin);

    EXPECT_EQ(test_case.expected_suitable, actual.has_value())
        << test_case.origin;

    if (test_case.expected_suitable) {
      EXPECT_EQ(test_case.origin, *actual.value());
      EXPECT_TRUE(actual->IsValid());
    }
  }
}

TEST(SuitableOriginTest, Deserialize_Serialize) {
  const struct {
    std::string_view str;
    std::optional<url::Origin> expected;
    const char* expected_serialization;
  } kTestCases[] = {
      {
          "",
          std::nullopt,
          nullptr,
      },
      {
          "http://a.test",
          std::nullopt,
          nullptr,
      },
      {
          "https://a.test",
          url::Origin::Create(GURL("https://a.test")),
          "https://a.test",
      },
      {
          "http://localhost",
          url::Origin::Create(GURL("http://localhost")),
          "http://localhost",
      },
      {
          "https://a.test/path?x=y#z",
          url::Origin::Create(GURL("https://a.test")),
          "https://a.test",
      },
      {
          "ws://a.test",
          std::nullopt,
          nullptr,
      },
      {
          "wss://a.test",
          std::nullopt,
          nullptr,
      },
  };

  for (const auto& test_case : kTestCases) {
    std::optional<SuitableOrigin> actual =
        SuitableOrigin::Deserialize(test_case.str);

    EXPECT_EQ(test_case.expected.has_value(), actual.has_value())
        << test_case.str;

    if (test_case.expected.has_value()) {
      EXPECT_EQ(test_case.expected, *actual.value()) << test_case.str;
      EXPECT_EQ(test_case.expected_serialization, actual->Serialize())
          << test_case.str;
    }
  }
}

TEST(SuitableOriginTest, Comparison) {
  const auto origin_a = SuitableOrigin::Deserialize("https://a.test");
  const auto origin_b = SuitableOrigin::Deserialize("https://b.test");

  EXPECT_LT(origin_a, origin_b);
  EXPECT_GT(origin_b, origin_a);

  EXPECT_LE(origin_a, origin_b);
  EXPECT_LE(origin_a, origin_a);

  EXPECT_GE(origin_b, origin_a);
  EXPECT_GE(origin_b, origin_b);

  EXPECT_EQ(origin_a, origin_a);
  EXPECT_NE(origin_a, origin_b);
}

TEST(SuitableOriginTest, IsSitePotentiallySuitable) {
  const struct {
    net::SchemefulSite site;
    bool expected_suitable;
  } kTestCases[] = {
      {
          net::SchemefulSite::Deserialize("https://a.test"),
          true,
      },
      {
          net::SchemefulSite::Deserialize("http://a.test"),
          true,
      },
      {
          net::SchemefulSite::Deserialize("https://localhost"),
          true,
      },
      {
          net::SchemefulSite::Deserialize("http://localhost"),
          true,
      },
      {
          net::SchemefulSite::Deserialize("https://127.0.0.1"),
          true,
      },
      {
          net::SchemefulSite::Deserialize("http://127.0.0.1"),
          true,
      },
      {
          net::SchemefulSite(),
          false,
      },
      {
          net::SchemefulSite::Deserialize("wss://a.test"),
          false,
      },
      {
          net::SchemefulSite::Deserialize("ws://a.test"),
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(IsSitePotentiallySuitable(test_case.site),
              test_case.expected_suitable)
        << test_case.site;
  }
}

}  // namespace
}  // namespace attribution_reporting

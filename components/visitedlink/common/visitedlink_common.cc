// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visitedlink/common/visitedlink_common.h"

#include <string.h>  // for memset()

#include <ostream>
#include <string_view>

#include "base/bit_cast.h"
#include "base/check.h"
#include "base/hash/md5.h"
#include "base/notreached.h"
#include "components/visitedlink/core/visited_link.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
visitedlink::VisitedLinkCommon::Fingerprint ConvertDigestToFingerprint(
    base::MD5Digest digest) {
  // This is the same as "return *(Fingerprint*)&digest.a;" but if we do that
  // direct cast the alignment could be wrong, and we can't access a 64-bit int
  // on arbitrary alignment on some processors. This reinterpret_casts it
  // down to a char array of the same size as fingerprint, and then does the
  // bit cast, which amounts to a memcpy. This does not handle endian issues.
  return base::bit_cast<visitedlink::VisitedLinkCommon::Fingerprint,
                        uint8_t[8]>(
      *reinterpret_cast<uint8_t(*)[8]>(&digest.a));
}
}  // namespace

namespace visitedlink {

const VisitedLinkCommon::Fingerprint VisitedLinkCommon::null_fingerprint_ = 0;
const VisitedLinkCommon::Hash VisitedLinkCommon::null_hash_ = -1;

VisitedLinkCommon::VisitedLinkCommon() {
  memset(salt_, 0, sizeof(salt_));
}

VisitedLinkCommon::~VisitedLinkCommon() = default;

// FIXME: this uses linear probing, it should be replaced with quadratic
// probing or something better. See VisitedLinkWriter::AddFingerprint
bool VisitedLinkCommon::IsVisited(std::string_view canonical_url) const {
  if (canonical_url.size() == 0) {
    return false;
  }
  if (!hash_table_ || table_length_ == 0) {
    return false;
  }
  return IsVisited(ComputeURLFingerprint(canonical_url));
}

bool VisitedLinkCommon::IsVisited(const GURL& url) const {
  return IsVisited(url.spec());
}

bool VisitedLinkCommon::IsVisited(const VisitedLink& link, uint64_t salt) {
  if (!hash_table_ || table_length_ == 0) {
    return false;
  }
  if (!link.IsValid()) {
    return false;
  }
  return IsVisited(ComputePartitionedFingerprint(
      link.link_url, link.top_level_site, link.frame_origin, salt));
}

bool VisitedLinkCommon::IsVisited(const GURL& link_url,
                                  const net::SchemefulSite& top_level_site,
                                  const url::Origin& frame_origin,
                                  uint64_t salt) {
  const VisitedLink link = {link_url, top_level_site, frame_origin};
  return IsVisited(link, salt);
}

bool VisitedLinkCommon::IsVisited(Fingerprint fingerprint) const {
  // Go through the table until we find the item or an empty spot (meaning it
  // wasn't found). This loop will terminate as long as the table isn't full,
  // which should be enforced by AddFingerprint.
  Hash first_hash = HashFingerprint(fingerprint);
  Hash cur_hash = first_hash;
  while (true) {
    Fingerprint cur_fingerprint = FingerprintAt(cur_hash);
    if (cur_fingerprint == null_fingerprint_)
      return false;  // End of probe sequence found.
    if (cur_fingerprint == fingerprint)
      return true;  // Found a match.

    // This spot was taken, but not by the item we're looking for, search in
    // the next position.
    cur_hash++;
    if (cur_hash == table_length_)
      cur_hash = 0;
    if (cur_hash == first_hash) {
      // Wrapped around and didn't find an empty space, this means we're in an
      // infinite loop because AddFingerprint didn't do its job resizing.
      NOTREACHED_IN_MIGRATION();
      return false;
    }
  }
}

// Uses the top 64 bits of the MD5 sum of the canonical URL as the fingerprint,
// this is as random as any other subset of the MD5SUM.
//
// FIXME: this uses the MD5SUM of the 16-bit character version. For systems
// where wchar_t is not 16 bits (Linux uses 32 bits, I think), this will not be
// compatable. We should define explicitly what should happen here across
// platforms, and convert if necessary (probably to UTF-16).

// static
VisitedLinkCommon::Fingerprint VisitedLinkCommon::ComputeURLFingerprint(
    std::string_view canonical_url,
    const uint8_t salt[LINK_SALT_LENGTH]) {
  DCHECK(canonical_url.size() > 0) << "Canonical URLs should not be empty";

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, std::string_view(reinterpret_cast<const char*>(salt),
                                         LINK_SALT_LENGTH));
  base::MD5Update(&ctx, canonical_url);

  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);

  return ConvertDigestToFingerprint(digest);
}

// static
VisitedLinkCommon::Fingerprint VisitedLinkCommon::ComputePartitionedFingerprint(
    const VisitedLink& link,
    uint64_t salt) {
  return ComputePartitionedFingerprint(link.link_url, link.top_level_site,
                                       link.frame_origin, salt);
}

// static
VisitedLinkCommon::Fingerprint VisitedLinkCommon::ComputePartitionedFingerprint(
    const GURL& link_url,
    const net::SchemefulSite& top_level_site,
    const url::Origin& frame_origin,
    uint64_t salt) {
  DCHECK(link_url.spec().size()) << "link_url should not be empty.";
  DCHECK(!top_level_site.opaque())
      << "Do not call ComputePartitionedFingerprint with an opaque top-level "
         "site.";
  DCHECK(!frame_origin.opaque()) << "Do not call ComputePartitionedFingerprint "
                                    "with an opaque frame origin.";

  base::MD5Context ctx;
  base::MD5Init(&ctx);

  // Salt the hash.
  base::MD5Update(&ctx, std::string_view(reinterpret_cast<const char*>(&salt),
                                         sizeof(salt)));

  // Add the link url.
  base::MD5Update(
      &ctx, std::string_view(link_url.spec().data(), link_url.spec().size()));

  // Add the serialized schemeful top-level site.
  const std::string serialized_site = top_level_site.Serialize();
  base::MD5Update(
      &ctx, std::string_view(serialized_site.data(), serialized_site.size()));

  // Add the serialized frame origin.
  const std::string serialized_origin = frame_origin.Serialize();
  base::MD5Update(&ctx, std::string_view(serialized_origin.data(),
                                         serialized_origin.size()));
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);

  return ConvertDigestToFingerprint(digest);
}

}  // namespace visitedlink

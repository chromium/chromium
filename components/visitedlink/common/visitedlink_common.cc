// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visitedlink/common/visitedlink_common.h"

#include <string.h>  // for memset()

#include <ostream>
#include <string_view>

#include "base/bit_cast.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "components/visitedlink/core/visited_link.h"
#include "crypto/obsolete/md5.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
visitedlink::VisitedLinkCommon::Fingerprint ConvertDigestToFingerprint(
    std::array<uint8_t, crypto::obsolete::Md5::kSize> digest) {
  auto first = base::span(digest).first<sizeof(uint64_t)>();
  return base::U64FromNativeEndian(first);
}
}  // namespace

namespace visitedlink {

crypto::obsolete::Md5 MakeMd5HasherForVisitedLink() {
  return {};
}

const VisitedLinkCommon::Fingerprint VisitedLinkCommon::null_fingerprint_ = 0;
const VisitedLinkCommon::Hash VisitedLinkCommon::null_hash_ = -1;

VisitedLinkCommon::VisitedLinkCommon() = default;

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
  return IsVisited(ComputePartitionedFingerprint(link, salt));
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
      NOTREACHED();
    }
  }
}

// Uses the top 64 bits of the MD5 sum of the canonical URL as the fingerprint,
// this is as random as any other subset of the MD5SUM.

// static
VisitedLinkCommon::Fingerprint VisitedLinkCommon::ComputeURLFingerprint(
    std::string_view canonical_url,
    const uint8_t salt[LINK_SALT_LENGTH]) {
  DCHECK(canonical_url.size() > 0) << "Canonical URLs should not be empty";

  auto md5 = MakeMd5HasherForVisitedLink();
  UNSAFE_BUFFERS(
      // SAFETY: salt is a reference to a local array which we know is the right
      // size.
      md5.Update(base::span<const uint8_t>(
          salt, base::checked_cast<size_t>(LINK_SALT_LENGTH)));)
  md5.Update(canonical_url);
  return ConvertDigestToFingerprint(md5.Finish());
}

// static
VisitedLinkCommon::Fingerprint VisitedLinkCommon::ComputePartitionedFingerprint(
    const VisitedLink& link,
    uint64_t salt) {
  return ComputePartitionedFingerprint(
      link.link_url.spec(), link.top_level_site, link.frame_origin, salt);
}

// static
VisitedLinkCommon::Fingerprint VisitedLinkCommon::ComputePartitionedFingerprint(
    std::string_view canonical_link_url,
    const net::SchemefulSite& top_level_site,
    const url::Origin& frame_origin,
    uint64_t salt) {
  DCHECK(canonical_link_url.size() > 0)
      << "Canonical link URLs should not be empty";
  DCHECK(!top_level_site.opaque())
      << "Do not call ComputePartitionedFingerprint with an opaque top-level "
         "site.";
  DCHECK(!frame_origin.opaque()) << "Do not call ComputePartitionedFingerprint "
                                    "with an opaque frame origin.";

  auto md5 = MakeMd5HasherForVisitedLink();

  // Salt the hash.
  md5.Update(base::byte_span_from_ref(salt));

  // Add the link url.
  md5.Update(canonical_link_url);

  // Add the serialized schemeful top-level site.
  md5.Update(top_level_site.Serialize());

  // Add the serialized frame origin.
  md5.Update(frame_origin.Serialize());
  return ConvertDigestToFingerprint(md5.Finish());
}

}  // namespace visitedlink

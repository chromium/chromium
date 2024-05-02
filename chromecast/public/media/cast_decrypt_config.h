// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_CAST_DECRYPT_CONFIG_H_
#define CHROMECAST_PUBLIC_MEDIA_CAST_DECRYPT_CONFIG_H_

#include <stdint.h>
#include <string>
#include <vector>

namespace chromecast {
namespace media {

// Specification of whether and how the stream is encrypted (in whole or part).
//
// Algorithm and mode that was used to encrypt the stream.
enum class EncryptionScheme { kUnencrypted, kAesCtr, kAesCbc };

// CENC 3rd Edition adds pattern encryption, through two new protection
// schemes: 'cens' (with AES-CTR) and 'cbcs' (with AES-CBC).
// The pattern applies independently to each 'encrypted' part of the frame (as
// defined by the relevant subsample entries), and reduces further the
// actual encryption applied through a repeating pattern of (encrypt:skip)
// 16 byte blocks. For example, in a (1:9) pattern, the first block is
// encrypted, and the next nine are skipped. This pattern is applied
// repeatedly until the end of the last 16-byte block in the subsample.
// Any remaining bytes are left clear.
// If either of encrypt_blocks or skip_blocks is 0, pattern encryption is
// disabled.
struct EncryptionPattern {
  EncryptionPattern() = default;
  EncryptionPattern(uint32_t encrypt_blocks, uint32_t skip_blocks);
  bool IsInEffect() const;

  uint32_t encrypt_blocks = 0;
  uint32_t skip_blocks = 0;
};

inline EncryptionPattern::EncryptionPattern(uint32_t encrypt_blocks,
                                            uint32_t skip_blocks)
    : encrypt_blocks(encrypt_blocks), skip_blocks(skip_blocks) {}

inline bool EncryptionPattern::IsInEffect() const {
  return encrypt_blocks != 0 && skip_blocks != 0;
}

// The Common Encryption spec provides for subsample encryption, where portions
// of a sample are set in cleartext. A SubsampleEntry specifies the number of
// clear and encrypted bytes in each subsample. For decryption, all of the
// encrypted bytes in a sample should be considered a single logical stream,
// regardless of how they are divided into subsamples, and the clear bytes
// should not be considered as part of decryption. This is logically equivalent
// to concatenating all 'cypher_bytes' portions of subsamples, decrypting that
// result, and then copying each byte from the decrypted block over the
// position of the corresponding encrypted byte.
struct SubsampleEntry {
  SubsampleEntry() : clear_bytes(0), cypher_bytes(0) {}
  SubsampleEntry(uint32_t clear_bytes, uint32_t cypher_bytes)
      : clear_bytes(clear_bytes), cypher_bytes(cypher_bytes) {}
  uint32_t clear_bytes;
  uint32_t cypher_bytes;
};

// Contains all metadata needed to decrypt a media sample.
class CastDecryptConfig {
 public:
  virtual ~CastDecryptConfig() = default;

  // Returns the ID for this sample's decryption key.
  virtual const std::string& key_id() const = 0;

  // Returns the initialization vector as defined by the encryption format.
  virtual const std::string& iv() const = 0;

  // Returns the encryption pattern for current sample.
  virtual const EncryptionPattern& pattern() const = 0;

  // Returns the clear and encrypted portions of the sample as described above.
  virtual const std::vector<SubsampleEntry>& subsamples() const = 0;

  // Returns the encryption scheme for this sample.
  virtual EncryptionScheme encryption_scheme() const = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_CAST_DECRYPT_CONFIG_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gcm_driver/crypto/encryption_header_parsers.h"

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const uint64_t kDefaultRecordSize = 4096;

TEST(EncryptionHeaderParsersTest, ParseValidEncryptionHeaders) {
  struct {
    const char* const header;
    const char* const parsed_keyid;
    const char* const parsed_salt;
    uint64_t parsed_rs;
  } expected_results[] = {
    { "keyid=foo;salt=c2l4dGVlbmNvb2xieXRlcw;rs=1024",
      "foo", "sixteencoolbytes", 1024 },
    { "keyid=foo; salt=c2l4dGVlbmNvb2xieXRlcw; rs=1024",
      "foo", "sixteencoolbytes", 1024 },
    { "KEYID=foo;SALT=c2l4dGVlbmNvb2xieXRlcw;RS=1024",
      "foo", "sixteencoolbytes", 1024 },
    { " keyid = foo ; salt = c2l4dGVlbmNvb2xieXRlcw ; rs = 1024 ",
      "foo", "sixteencoolbytes", 1024 },
    { "keyid=foo", "foo", "", kDefaultRecordSize },
    { "keyid=foo;", "foo", "", kDefaultRecordSize },
    { "keyid=\"foo\"", "foo", "", kDefaultRecordSize },
    { "salt=c2l4dGVlbmNvb2xieXRlcw",
      "", "sixteencoolbytes", kDefaultRecordSize },
    { "rs=2048", "", "", 2048 },
    { "keyid=foo;someothervalue=1;rs=42", "foo", "", 42 },
  };

  for (size_t i = 0; i < std::size(expected_results); i++) {
    SCOPED_TRACE(i);

    std::string header(expected_results[i].header);

    EncryptionHeaderIterator iterator(header.begin(), header.end());
    ASSERT_TRUE(iterator.GetNext());

    EXPECT_EQ(expected_results[i].parsed_keyid, iterator.keyid());
    EXPECT_EQ(expected_results[i].parsed_salt, iterator.salt());
    EXPECT_EQ(expected_results[i].parsed_rs, iterator.rs());

    EXPECT_FALSE(iterator.GetNext());
  }
}

TEST(EncryptionHeaderParsersTest, ParseValidMultiValueEncryptionHeaders) {
  const size_t kNumberOfValues = 2u;

  struct {
    const char* const header;
    struct {
      const char* const keyid;
      const char* const salt;
      uint64_t rs;
    } parsed_values[kNumberOfValues];
  } expected_results[] = {
    { "keyid=foo;salt=c2l4dGVlbmNvb2xieXRlcw;rs=1024,keyid=foo;salt=c2l4dGVlbm"
          "Nvb2xieXRlcw;rs=1024",
      { { "foo", "sixteencoolbytes", 1024 },
        { "foo", "sixteencoolbytes", 1024 } } },
    { "keyid=foo,salt=c2l4dGVlbmNvb2xieXRlcw;rs=1024",
      { { "foo", "", kDefaultRecordSize },
        { "", "sixteencoolbytes", 1024 } } },
    { "keyid=foo,keyid=bar;salt=c2l4dGVlbmNvb2xieXRlcw;rs=1024",
      { { "foo", "", kDefaultRecordSize },
        { "bar", "sixteencoolbytes", 1024 } } },
    { "keyid=\"foo,keyid=bar\",salt=c2l4dGVlbmNvb2xieXRlcw",
      { { "foo,keyid=bar", "", kDefaultRecordSize },
        { "", "sixteencoolbytes", kDefaultRecordSize } } },
  };

  for (size_t i = 0; i < std::size(expected_results); i++) {
    SCOPED_TRACE(i);

    std::string header(expected_results[i].header);

    EncryptionHeaderIterator iterator(header.begin(), header.end());
    for (size_t j = 0; j < kNumberOfValues; ++j) {
      ASSERT_TRUE(iterator.GetNext());

      EXPECT_EQ(expected_results[i].parsed_values[j].keyid, iterator.keyid());
      EXPECT_EQ(expected_results[i].parsed_values[j].salt, iterator.salt());
      EXPECT_EQ(expected_results[i].parsed_values[j].rs, iterator.rs());
    }

    EXPECT_FALSE(iterator.GetNext());
  }
}

TEST(EncryptionHeaderParsersTest, ParseInvalidEncryptionHeaders) {
  const char* const expected_failures[] = {
    // Values in the name-value pairs are not optional.
    "keyid",
    "keyid=",
    "keyid=foo;keyid",
    "salt",
    "salt=",
    "rs",
    "rs=",

    // Supplying the same name multiple times in the same value is invalid.
    "keyid=foo;keyid=bar",
    "keyid=foo;bar=baz;keyid=qux",

    // The salt must be a URL-safe base64 decodable string.
    "salt=YmV/2ZXJ-sMDA",
    "salt=dHdlbHZlY29vbGJ5dGVz=====",
    "salt=c2l4dGVlbmNvb2xieXRlcw;salt=123$xyz",
    "salt=123$xyz",

    // The record size must be a positive decimal integer greater than one that
    // does not start with a plus.
    "rs=0",
    "rs=0x13",
    "rs=1",
    "rs=-1",
    "rs=+5",
    "rs=99999999999999999999999999999999",
    "rs=foobar",
  };

  const char* const expected_failures_second_iter[] = {
    // Valid first field, missing value in the second field.
    "keyid=foo,novaluekey",

    // Valid first field, undecodable salt in the second field.
    "salt=c2l4dGVlbmNvb2xieXRlcw,salt=123$xyz",

    // Valid first field, invalid record size in the second field.
    "rs=2,rs=0",
  };

  for (size_t i = 0; i < std::size(expected_failures); i++) {
    SCOPED_TRACE(i);

    std::string header(expected_failures[i]);

    EncryptionHeaderIterator iterator(header.begin(), header.end());
    EXPECT_FALSE(iterator.GetNext());
  }

  for (size_t i = 0; i < std::size(expected_failures_second_iter); i++) {
    SCOPED_TRACE(i);

    std::string header(expected_failures_second_iter[i]);

    EncryptionHeaderIterator iterator(header.begin(), header.end());
    EXPECT_TRUE(iterator.GetNext());
    EXPECT_FALSE(iterator.GetNext());
  }
}

TEST(EncryptionHeaderParsersTest, ParseValidCryptoKeyHeaders) {
  struct {
    const char* const header;
    const char* const parsed_keyid;
    const char* const parsed_aesgcm128;
    const char* const parsed_dh;
  } expected_results[] = {
    { "keyid=foo;aesgcm128=c2l4dGVlbmNvb2xieXRlcw;dh=dHdlbHZlY29vbGJ5dGVz",
      "foo", "sixteencoolbytes", "twelvecoolbytes" },
    { "keyid=foo; aesgcm128=c2l4dGVlbmNvb2xieXRlcw; dh=dHdlbHZlY29vbGJ5dGVz",
      "foo", "sixteencoolbytes", "twelvecoolbytes" },
    { "keyid = foo ; aesgcm128 = c2l4dGVlbmNvb2xieXRlcw ; dh = dHdlbHZlY29vbGJ5"
          "dGVz ",
      "foo", "sixteencoolbytes", "twelvecoolbytes" },
    { "KEYID=foo;AESGCM128=c2l4dGVlbmNvb2xieXRlcw;DH=dHdlbHZlY29vbGJ5dGVz",
      "foo", "sixteencoolbytes", "twelvecoolbytes" },
    { "keyid=foo", "foo", "", "" },
    { "aesgcm128=c2l4dGVlbmNvb2xieXRlcw", "", "sixteencoolbytes", "" },
    { "aesgcm128=\"c2l4dGVlbmNvb2xieXRlcw\"", "", "sixteencoolbytes", "" },
    { "dh=dHdlbHZlY29vbGJ5dGVz", "", "", "twelvecoolbytes" },
    { "keyid=foo;someothervalue=bar;aesgcm128=dHdlbHZlY29vbGJ5dGVz",
      "foo", "twelvecoolbytes", "" },
  };

  for (size_t i = 0; i < std::size(expected_results); i++) {
    SCOPED_TRACE(i);

    std::string header(expected_results[i].header);

    CryptoKeyHeaderIterator iterator(header.begin(), header.end());
    ASSERT_TRUE(iterator.GetNext());

    EXPECT_EQ(expected_results[i].parsed_keyid, iterator.keyid());
    EXPECT_EQ(expected_results[i].parsed_aesgcm128, iterator.aesgcm128());
    EXPECT_EQ(expected_results[i].parsed_dh, iterator.dh());

    EXPECT_FALSE(iterator.GetNext());
  }
}

TEST(EncryptionHeaderParsersTest, ParseValidMultiValueCryptoKeyHeaders) {
  const size_t kNumberOfValues = 2u;

  struct {
    const char* const header;
    struct {
      const char* const keyid;
      const char* const aesgcm128;
      const char* const dh;
    } parsed_values[kNumberOfValues];
  } expected_results[] = {
    { "keyid=foo;aesgcm128=c2l4dGVlbmNvb2xieXRlcw;dh=dHdlbHZlY29vbGJ5dGVz,"
          "keyid=bar;aesgcm128=dHdlbHZlY29vbGJ5dGVz;dh=c2l4dGVlbmNvb2xieXRlcw",
      { { "foo", "sixteencoolbytes", "twelvecoolbytes" },
        { "bar", "twelvecoolbytes", "sixteencoolbytes" } } },
    { "keyid=foo,aesgcm128=c2l4dGVlbmNvb2xieXRlcw",
      { { "foo", "", "" },
        { "", "sixteencoolbytes", "" } } },
    { "keyid=foo,keyid=bar;dh=dHdlbHZlY29vbGJ5dGVz",
      { { "foo", "", "" },
        { "bar", "", "twelvecoolbytes" } } },
    { "keyid=\"foo,keyid=bar\",aesgcm128=c2l4dGVlbmNvb2xieXRlcw",
      { { "foo,keyid=bar", "", "" },
        { "", "sixteencoolbytes", "" } } },
  };

  for (size_t i = 0; i < std::size(expected_results); i++) {
    SCOPED_TRACE(i);

    std::string header(expected_results[i].header);

    CryptoKeyHeaderIterator iterator(header.begin(), header.end());
    for (size_t j = 0; j < kNumberOfValues; ++j) {
      ASSERT_TRUE(iterator.GetNext());

      EXPECT_EQ(expected_results[i].parsed_values[j].keyid, iterator.keyid());
      EXPECT_EQ(expected_results[i].parsed_values[j].aesgcm128,
                iterator.aesgcm128());
      EXPECT_EQ(expected_results[i].parsed_values[j].dh, iterator.dh());
    }

    EXPECT_FALSE(iterator.GetNext());
  }
}

TEST(EncryptionHeaderParsersTest, ParseInvalidCryptoKeyHeaders) {
  const char* const expected_failures[] = {
    // Values in the name-value pairs are not optional.
    "keyid",
    "keyid=",
    "keyid=foo;keyid",
    "aesgcm128",
    "aesgcm128=",
    "dh",
    "dh=",

    // Supplying the same name multiple times in the same value is invalid.
    "keyid=foo;keyid=bar",
    "keyid=foo;bar=baz;keyid=qux",

    // The "aesgcm128" parameter must be a URL-safe base64 decodable string.
    "aesgcm128=123$xyz",
    "aesgcm128=foobar;aesgcm128=123$xyz",

    // The "dh" parameter must be a URL-safe base64 decodable string.
    "dh=YmV/2ZXJ-sMDA",
    "dh=dHdlbHZlY29vbGJ5dGVz=====",
    "dh=123$xyz",
  };

  const char* const expected_failures_second_iter[] = {
    // Valid first field, missing value in the second field.
    "keyid=foo,novaluekey",

    // Valid first field, undecodable aesgcm128 value in the second field.
    "dh=dHdlbHZlY29vbGJ5dGVz,aesgcm128=123$xyz",
  };

  for (size_t i = 0; i < std::size(expected_failures); i++) {
    SCOPED_TRACE(i);

    std::string header(expected_failures[i]);

    CryptoKeyHeaderIterator iterator(header.begin(), header.end());
    EXPECT_FALSE(iterator.GetNext());
  }

  for (size_t i = 0; i < std::size(expected_failures_second_iter); i++) {
    SCOPED_TRACE(i);

    std::string header(expected_failures_second_iter[i]);

    CryptoKeyHeaderIterator iterator(header.begin(), header.end());
    EXPECT_TRUE(iterator.GetNext());
    EXPECT_FALSE(iterator.GetNext());
  }
}

TEST(EncryptionHeaderParsersTest, SixValueHeader) {
  const std::string header("keyid=0,keyid=1,keyid=2,keyid=3,keyid=4,keyid=5");

  EncryptionHeaderIterator encryption_iterator(header.begin(), header.end());
  CryptoKeyHeaderIterator crypto_key_iterator(header.begin(), header.end());

  for (size_t i = 0; i < 6; ++i) {
    SCOPED_TRACE(i);

    ASSERT_TRUE(encryption_iterator.GetNext());
    ASSERT_TRUE(crypto_key_iterator.GetNext());
  }

  EXPECT_FALSE(encryption_iterator.GetNext());
  EXPECT_FALSE(crypto_key_iterator.GetNext());
}

TEST(EncryptionHeaderParsersTest, InvalidHeadersResetOutput) {
  // Valid first field, invalid record size parameter in the second field.
  const std::string encryption_header(
      "keyid=foo;salt=c2l4dGVlbmNvb2xieXRlcw;rs=1024,rs=foobar");

  // Valid first field, undecodable aesgcm128 parameter in the second field.
  const std::string crypto_key_header(
      "keyid=foo;aesgcm128=c2l4dGVlbmNvb2xieXRlcw;dh=dHdlbHZlY29vbGJ5dGVz,"
      "aesgcm128=$$$");

  EncryptionHeaderIterator encryption_iterator(
      encryption_header.begin(), encryption_header.end());

  ASSERT_EQ(0u, encryption_iterator.keyid().size());
  ASSERT_EQ(0u, encryption_iterator.salt().size());
  ASSERT_EQ(4096u, encryption_iterator.rs());

  ASSERT_TRUE(encryption_iterator.GetNext());

  EXPECT_EQ("foo", encryption_iterator.keyid());
  EXPECT_EQ("sixteencoolbytes", encryption_iterator.salt());
  EXPECT_EQ(1024u, encryption_iterator.rs());

  ASSERT_FALSE(encryption_iterator.GetNext());

  EXPECT_EQ(0u, encryption_iterator.keyid().size());
  EXPECT_EQ(0u, encryption_iterator.salt().size());
  EXPECT_EQ(4096u, encryption_iterator.rs());

  CryptoKeyHeaderIterator crypto_key_iterator(
      crypto_key_header.begin(), crypto_key_header.end());

  ASSERT_EQ(0u, crypto_key_iterator.keyid().size());
  ASSERT_EQ(0u, crypto_key_iterator.aesgcm128().size());
  ASSERT_EQ(0u, crypto_key_iterator.dh().size());

  ASSERT_TRUE(crypto_key_iterator.GetNext());

  EXPECT_EQ("foo", crypto_key_iterator.keyid());
  EXPECT_EQ("sixteencoolbytes", crypto_key_iterator.aesgcm128());
  EXPECT_EQ("twelvecoolbytes", crypto_key_iterator.dh());

  ASSERT_FALSE(crypto_key_iterator.GetNext());

  EXPECT_EQ(0u, crypto_key_iterator.keyid().size());
  EXPECT_EQ(0u, crypto_key_iterator.aesgcm128().size());
  EXPECT_EQ(0u, crypto_key_iterator.dh().size());
}

}  // namespace

}  // namespace gcm

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use web_package_rust::http::{
    is_delimiter, is_field_vchar, is_token_char, is_valid_header_name, is_valid_header_value,
    DELIMITERS, HTAB, SP,
};

#[gtest(WebPackageRustTest, TestHttpIsDelimiter)]
fn test_http_is_delimiter() {
    // RFC 9110 Section 5.6.2 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.6.2):
    // DQUOTE and "(),/:;<=>?@[\]{}"
    for &d in DELIMITERS {
        expect_true!(is_delimiter(d));
    }

    // Other VCHAR characters that are valid tchars must not be delimiters.
    let tchar_samples =
        b"!#$%&'*+-.^_`|~0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for &c in tchar_samples {
        expect_false!(is_delimiter(c));
    }

    // Whitespace and control chars are not delimiters.
    expect_false!(is_delimiter(SP));
    expect_false!(is_delimiter(HTAB));
    expect_false!(is_delimiter(0x00));
    expect_false!(is_delimiter(0x7F));
    expect_false!(is_delimiter(0x80));
}

#[gtest(WebPackageRustTest, TestHttpIsTokenChar)]
fn test_http_is_token_char() {
    // RFC 9110 Section 5.6.2 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.6.2):
    // any VCHAR (%x21-7E), except delimiters.
    for b in 0u8..=255 {
        let expected = b.is_ascii_graphic() && !is_delimiter(b);
        expect_eq!(is_token_char(b), expected);
    }
}

#[gtest(WebPackageRustTest, TestHttpIsValidHeaderName)]
fn test_http_is_valid_header_name() {
    // RFC 9110 Section 5.1 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.1):
    // `field-name = token` (1*tchar). Must not be empty.
    expect_false!(is_valid_header_name(b""));

    // Single-character valid names.
    expect_true!(is_valid_header_name(b"a"));
    expect_true!(is_valid_header_name(b"z"));
    expect_true!(is_valid_header_name(b"0"));
    expect_true!(is_valid_header_name(b"9"));
    expect_true!(is_valid_header_name(b"-"));
    expect_true!(is_valid_header_name(b"_"));

    // Single-character invalid names.
    expect_false!(is_valid_header_name(b"A"));
    expect_false!(is_valid_header_name(b"Z"));
    expect_false!(is_valid_header_name(b":"));
    expect_false!(is_valid_header_name(b" "));
    expect_false!(is_valid_header_name(b"\t"));
    expect_false!(is_valid_header_name(b"/"));
    expect_false!(is_valid_header_name(b"\0"));

    // Valid lowercase field names.
    expect_true!(is_valid_header_name(b"content-type"));
    expect_true!(is_valid_header_name(b"accept"));
    expect_true!(is_valid_header_name(b"x-custom_header.v1"));
    expect_true!(is_valid_header_name(b"!#$%&'*+-.^_`|~"));

    // RFC 9113 Section 8.2.1 (https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1):
    // Disallow uppercase ASCII characters (0x41-0x5A).
    // Test uppercase at first, middle, and last character positions.
    expect_false!(is_valid_header_name(b"Content-type"));
    expect_false!(is_valid_header_name(b"content-Type"));
    expect_false!(is_valid_header_name(b"content-typE"));
    expect_false!(is_valid_header_name(b"ACCEPT"));

    // RFC 9113 Section 8.2.1 (https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1):
    // Disallow colon (0x3A) in field names.
    // Test pseudo-headers (colon at start) and internal / trailing colons.
    expect_false!(is_valid_header_name(b":status"));
    expect_false!(is_valid_header_name(b":path"));
    expect_false!(is_valid_header_name(b":method"));
    expect_false!(is_valid_header_name(b":authority"));
    expect_false!(is_valid_header_name(b"foo:bar"));
    expect_false!(is_valid_header_name(b"foobar:"));

    // RFC 9113 Section 8.2.1 (https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1):
    // Disallow ranges 0x00-0x20 (includes SP, HTAB, controls).
    expect_false!(is_valid_header_name(b" foo"));
    expect_false!(is_valid_header_name(b"foo bar"));
    expect_false!(is_valid_header_name(b"foo "));
    expect_false!(is_valid_header_name(b"\tfoo"));
    expect_false!(is_valid_header_name(b"foo\tbar"));
    expect_false!(is_valid_header_name(b"foo\t"));
    expect_false!(is_valid_header_name(b"foo\0bar"));
    expect_false!(is_valid_header_name(b"foo\rbar"));
    expect_false!(is_valid_header_name(b"foo\nbar"));
    expect_false!(is_valid_header_name(b"foo\x01bar"));
    expect_false!(is_valid_header_name(b"foo\x1Fbar"));

    // RFC 9113 Section 8.2.1 (https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1):
    // Disallow ranges 0x7F-0xFF.
    expect_false!(is_valid_header_name(b"foo\x7Fbar"));
    expect_false!(is_valid_header_name(b"foo\x80bar"));
    expect_false!(is_valid_header_name(b"foo\xFFbar"));
    expect_false!(is_valid_header_name("café".as_bytes()));

    // Delimiters (other than colon, tested above) are not valid token chars.
    expect_false!(is_valid_header_name(b"foo/bar"));
    expect_false!(is_valid_header_name(b"foo,bar"));
    expect_false!(is_valid_header_name(b"foo=bar"));
    expect_false!(is_valid_header_name(b"foo?bar"));
    expect_false!(is_valid_header_name(b"foo@bar"));
    expect_false!(is_valid_header_name(b"(foo)"));
    expect_false!(is_valid_header_name(b"<foo>"));
    expect_false!(is_valid_header_name(b"foo[bar]"));
    expect_false!(is_valid_header_name(b"\"foo\""));
}

#[gtest(WebPackageRustTest, TestHttpIsFieldVchar)]
fn test_http_is_field_vchar() {
    // RFC 9110 Section 5.5 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5):
    // `field-vchar = VCHAR / obs-text`
    // VCHAR = %x21-7E, obs-text = %x80-FF.
    for b in 0u8..=255 {
        let expected = b.is_ascii_graphic() || b >= 0x80;
        expect_eq!(is_field_vchar(&b), expected);
    }
}

#[gtest(WebPackageRustTest, TestHttpIsValidHeaderValue)]
fn test_http_is_valid_header_value() {
    // RFC 9110 Section 5.5 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5):
    // `field-value = *field-content` permits empty values.
    expect_true!(is_valid_header_value(b""));

    // Single character tests.
    expect_true!(is_valid_header_value(b"a"));
    expect_true!(is_valid_header_value(b"~"));
    expect_true!(is_valid_header_value(b"\x80"));
    expect_true!(is_valid_header_value(b"\xFF"));
    expect_false!(is_valid_header_value(b" "));
    expect_false!(is_valid_header_value(b"\t"));
    expect_false!(is_valid_header_value(b"\0"));
    expect_false!(is_valid_header_value(b"\r"));
    expect_false!(is_valid_header_value(b"\n"));
    expect_false!(is_valid_header_value(b"\x0C")); // Form feed
    expect_false!(is_valid_header_value(b"\x0B")); // Vertical tab

    // Valid typical field values.
    expect_true!(is_valid_header_value(b"text/plain"));
    expect_true!(is_valid_header_value(b"text/html; charset=utf-8"));
    expect_true!(is_valid_header_value(b"gzip, deflate, br"));
    expect_true!(is_valid_header_value(b"*/*"));

    // Internal SP and HTAB are allowed per RFC 9110 Section 5.5
    // (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5).
    expect_true!(is_valid_header_value(b"foo bar"));
    expect_true!(is_valid_header_value(b"foo\tbar"));
    expect_true!(is_valid_header_value(b"foo   bar"));
    expect_true!(is_valid_header_value(b"foo\t\tbar"));
    expect_true!(is_valid_header_value(b"foo \t bar"));
    expect_true!(is_valid_header_value(b"foo\t \tbar"));

    // RFC 9113 Section 8.2.1 (https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1):
    // MUST NOT start or end with SP or HTAB.
    expect_false!(is_valid_header_value(b" foo"));
    expect_false!(is_valid_header_value(b"   foo"));
    expect_false!(is_valid_header_value(b"\tfoo"));
    expect_false!(is_valid_header_value(b"\t\tfoo"));
    expect_false!(is_valid_header_value(b" \tfoo"));
    expect_false!(is_valid_header_value(b"foo "));
    expect_false!(is_valid_header_value(b"foo   "));
    expect_false!(is_valid_header_value(b"foo\t"));
    expect_false!(is_valid_header_value(b"foo\t\t"));
    expect_false!(is_valid_header_value(b"foo\t "));
    expect_false!(is_valid_header_value(b" foo "));
    expect_false!(is_valid_header_value(b"\tfoo\t"));

    // Whitespace only strings must be rejected.
    expect_false!(is_valid_header_value(b" "));
    expect_false!(is_valid_header_value(b"   "));
    expect_false!(is_valid_header_value(b"\t"));
    expect_false!(is_valid_header_value(b"\t\t"));
    expect_false!(is_valid_header_value(b" \t "));

    // RFC 9110 Section 5.5 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5) /
    // RFC 9113 Section 8.2.1 (https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1):
    // MUST NOT contain NUL, LF, or CR.
    expect_false!(is_valid_header_value(b"text/\0plain"));
    expect_false!(is_valid_header_value(b"text/\rplain"));
    expect_false!(is_valid_header_value(b"text/\nplain"));
    expect_false!(is_valid_header_value(b"\0foo"));
    expect_false!(is_valid_header_value(b"foo\0"));

    // RFC 9112 Section 5.2 (https://www.rfc-editor.org/rfc/rfc9112.html#section-5.2):
    // Obsolete line folding (`obs-fold = OWS CRLF RWS`) is rejected.
    expect_false!(is_valid_header_value(b"foo\r\n bar"));
    expect_false!(is_valid_header_value(b"foo\r\n\tbar"));
    expect_false!(is_valid_header_value(b"foo\r\n  bar"));
    expect_false!(is_valid_header_value(b"\r\n bar"));
    expect_false!(is_valid_header_value(b"foo\r\n"));
    expect_false!(is_valid_header_value(b"\r\n"));

    // Other ASCII control characters (0x01..=0x08, 0x0B, 0x0C, 0x0E..=0x1F, 0x7F)
    // are disallowed.
    expect_false!(is_valid_header_value(b"foo\x01bar"));
    expect_false!(is_valid_header_value(b"foo\x08bar"));
    expect_false!(is_valid_header_value(b"foo\x0Bbar")); // Vertical Tab
    expect_false!(is_valid_header_value(b"foo\x0Cbar")); // Form Feed
    expect_false!(is_valid_header_value(b"foo\x1Fbar"));
    expect_false!(is_valid_header_value(b"foo\x7Fbar")); // DEL

    // obs-text (%x80-FF) is permitted in field-vchar.
    expect_true!(is_valid_header_value(b"caf\xe9"));
    expect_true!(is_valid_header_value(b"\x80\xff"));
    expect_true!(is_valid_header_value(b"\x80abc"));
    expect_true!(is_valid_header_value(b"abc\x80"));
    expect_true!(is_valid_header_value("こんにちは".as_bytes()));
    expect_true!(is_valid_header_value("🚀".as_bytes()));

    // obs-text with leading or trailing whitespace is still rejected.
    expect_false!(is_valid_header_value(b" \x80"));
    expect_false!(is_valid_header_value(b"\x80 "));
}

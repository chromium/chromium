// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! HTTP syntactic validation helpers conforming to RFC 9110
//! (https://www.rfc-editor.org/rfc/rfc9110.html, HTTP Semantics), RFC 9112
//! (https://www.rfc-editor.org/rfc/rfc9112.html, HTTP/1.1), and RFC 9113
//! (https://www.rfc-editor.org/rfc/rfc9113.html, HTTP/2).

// RFC 5234 Appendix B.1 Core Rules (https://www.rfc-editor.org/rfc/rfc5234#appendix-B.1):
//
// HTAB           =  %x09
//                ; horizontal tab
// SP             =  %x20
// VCHAR          =  %x21-7E
//                ; visible (printing) characters
pub const HTAB: u8 = 0x09;
pub const SP: u8 = 0x20;

/// Delimiters per RFC 9110 Section 5.6.2 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.6.2):
///
/// delimiters = DQUOTE and "(),/:;<=>?@[\]{}"
///
/// RFC 5234 Appendix B.1 (https://www.rfc-editor.org/rfc/rfc5234#appendix-B.1):
///
/// DQUOTE         =  %x22
///                ; " (Double Quote)
pub const DELIMITERS: &[u8] = b"\x22(),/:;<=>?@[\\]{}";

// Static assert that u8::is_ascii_graphic() strictly matches RFC 5234 Appendix
// B.1 VCHAR = %x21-7E across all possible byte values.
// Note: A primitive `while` loop is used because in stable Rust, `for` loops
// desugar to `IntoIterator::into_iter` which is not available in `const`
// contexts.
const _: () = {
    const VCHAR_MIN: u8 = 0x21;
    const VCHAR_MAX: u8 = 0x7E;
    let mut b = 0u16;
    while b <= 255 {
        let byte = b as u8;
        let is_vchar = byte >= VCHAR_MIN && byte <= VCHAR_MAX;
        assert!(byte.is_ascii_graphic() == is_vchar);
        b += 1;
    }
};

/// Returns true if `b` is an HTTP delimiter per RFC 9110 Section 5.6.2:
///
/// https://www.rfc-editor.org/rfc/rfc9110.html#section-5.6.2
///
/// delimiters = DQUOTE and "(),/:;<=>?@[\]{}"
pub fn is_delimiter(b: u8) -> bool {
    DELIMITERS.contains(&b)
}

/// Checks whether a byte is a valid HTTP token character (`tchar`) per RFC 9110
/// Section 5.6.2:
///
/// https://www.rfc-editor.org/rfc/rfc9110.html#section-5.6.2
///
/// token          = 1*tchar
///
/// tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
///                / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
///                / DIGIT / ALPHA
///                ; any VCHAR, except delimiters
///
/// delimiters     = DQUOTE and "(),/:;<=>?@[\]{}"
pub fn is_token_char(b: u8) -> bool {
    // RFC 9110 Section 5.6.2 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.6.2):
    // "any VCHAR, except delimiters".
    // In RFC 5234 Section B.1 (https://www.rfc-editor.org/rfc/rfc5234#appendix-B.1),
    // `VCHAR = %x21-7E`, matched by `is_ascii_graphic()`.
    b.is_ascii_graphic() && !is_delimiter(b)
}

/// Validates that an HTTP field name conforms to RFC 9110 Section 5.1 and
/// RFC 9113 Section 8.2.1 rules.
///
/// https://www.rfc-editor.org/rfc/rfc9110.html#section-5.1
/// https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1
///
/// Per RFC 9110 Section 5.1 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.1):
/// `field-name = token`, where `token = 1*tchar`.
///
/// Per RFC 9113 Section 8.2.1 (https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1):
/// "An implementation that validates fields according to the definitions in
/// Sections 5.1 and 5.5 of [HTTP] only needs an additional check that field
/// names do not include uppercase characters."
pub fn is_valid_header_name(name: &[u8]) -> bool {
    !name.is_empty() && name.iter().all(|&b| is_token_char(b) && !b.is_ascii_uppercase())
}

/// Checks whether a byte is a valid `field-vchar` per RFC 9110 Section 5.5:
///
/// https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5
///
/// field-vchar    = VCHAR / obs-text
/// obs-text       = %x80-FF
pub fn is_field_vchar(b: &u8) -> bool {
    // VCHAR (%x21-7E)
    b.is_ascii_graphic()
    // obs-text (%x80-FF)
    || matches!(*b, 0x80..=0xFF)
}

/// Validates an HTTP field value per RFC 9110 Section 5.5 and RFC 9113 Section
/// 8.2.1.
///
/// https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5
/// https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1
///
/// ABNF grammar from RFC 9110 Section 5.5 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5):
///
/// field-value    = *field-content
/// field-content  = field-vchar [ 1*( SP / HTAB / field-vchar ) field-vchar ]
/// field-vchar    = VCHAR / obs-text
/// obs-text       = %x80-FF
///
/// Per RFC 9113 Section 8.2.1 (https://www.rfc-editor.org/rfc/rfc9113.html#section-8.2.1):
/// "An implementation that validates fields according to the definitions in
/// Sections 5.1 and 5.5 of [HTTP] only needs an additional check that field
/// names do not include uppercase characters."
// Character inside (not first and not last) of `field-content`.
fn is_field_content_char(b: &u8) -> bool {
    is_field_vchar(b) || matches!(*b, SP | HTAB)
}

// Validates `field-value` from the comment above.
pub fn is_valid_header_value(val: &[u8]) -> bool {
    // RFC 9110 Section 5.5 / RFC 9113 Section 8.2.1: Leading and trailing
    // SP/HTAB are forbidden in field-content. The first and last bytes must be
    // `field-vchar`, while interior bytes may be SP or HTAB.
    match val {
        [] => true,
        [b] => is_field_vchar(b),
        [first, mid @ .., last] => {
            is_field_vchar(first) && is_field_vchar(last) && mid.iter().all(is_field_content_char)
        }
    }
}

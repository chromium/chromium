// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/url_formatter.h"

#include <stddef.h>
#include <string.h>

#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/spoof_checks/idn_spoof_checker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace url_formatter {

namespace {

using base::ASCIIToUTF16;
using base::WideToUTF16;

// Expected result of the IDN conversion.
enum class Result {
  // Hostname can safely be decoded to unicode.
  kSafe,
  // Hostname is unsafe to be decoded to unicode.
  kUnsafe,
  // Hostname is invalid punycode, must not be decoded.
  kInvalid
};

// Alias the values to make the tests less verbose.
const Result kSafe = Result::kSafe;
const Result kUnsafe = Result::kUnsafe;
const Result kInvalid = Result::kInvalid;

struct IDNTestCase {
  // The IDNA/Punycode version of the domain (plain ASCII).
  const char* const input;
  // The equivalent Unicode version of the domain. Even if we expect the domain
  // to be displayed in Punycode, this should still contain the Unicode
  // equivalent (see |unicode_allowed|).
  const wchar_t* unicode_output;
  // Whether we expect the domain to be displayed decoded as a Unicode string or
  // in its Punycode form.
  const Result expected_result;
};

// These cases can be generated with the script
// tools/security/idn_test_case_generator.py.
// See documentation there: you can either run it from the command line or call
// the make_case function directly from the Python shell (which may be easier
// for entering Unicode text).
//
// Q: Why not just do this conversion right here in the test, rather than having
//    a Python script to generate it?
// A: Because then we would have to rely on complex logic (IDNA encoding) in the
//    test itself; the same code we are trying to test. By using Python's IDN
//    encoder to generate the test data, we independently verify that our
//    algorithm is correct.

// TODO(jshin): Replace L"..." with "..." in UTF-8 when it's easier to read.
const IDNTestCase kIdnCases[] = {
    // No IDN
    {"www.google.com", L"www.google.com", kSafe},
    {"www.google.com.", L"www.google.com.", kSafe},
    {".", L".", kSafe},
    {"", L"", kSafe},
    // Invalid IDN
    {"xn--example-.com", L"xn--example-.com", kInvalid},
    // IDN
    // Hanzi (Traditional Chinese)
    {"xn--1lq90ic7f1rc.cn", L"\x5317\x4eac\x5927\x5b78.cn", kSafe},
    // Hanzi ('video' in Simplified Chinese
    {"xn--cy2a840a.com", L"\x89c6\x9891.com", kSafe},
    // Hanzi + '123'
    {"www.xn--123-p18d.com",
     L"www.\x4e00"
     L"123.com",
     kSafe},
    // Hanzi + Latin : U+56FD is simplified
    {"www.xn--hello-9n1hm04c.com", L"www.hello\x4e2d\x56fd.com", kSafe},
    // Kanji + Kana (Japanese)
    {"xn--l8jvb1ey91xtjb.jp", L"\x671d\x65e5\x3042\x3055\x3072.jp", kSafe},
    // Katakana including U+30FC
    {"xn--tckm4i2e.jp", L"\x30b3\x30de\x30fc\x30b9.jp", kSafe},
    {"xn--3ck7a7g.jp", L"\u30ce\u30f3\u30bd.jp", kSafe},
    // Katakana + Latin (Japanese)
    {"xn--e-efusa1mzf.jp", L"e\x30b3\x30de\x30fc\x30b9.jp", kSafe},
    {"xn--3bkxe.jp", L"\x30c8\x309a.jp", kSafe},
    // Hangul (Korean)
    {"www.xn--or3b17p6jjc.kr", L"www.\xc804\xc790\xc815\xbd80.kr", kSafe},
    // b<u-umlaut>cher (German)
    {"xn--bcher-kva.de",
     L"b\x00fc"
     L"cher.de",
     kSafe},
    // a with diaeresis
    {"www.xn--frgbolaget-q5a.se", L"www.f\x00e4rgbolaget.se", kSafe},
    // c-cedilla (French)
    {"www.xn--alliancefranaise-npb.fr",
     L"www.alliancefran\x00e7"
     L"aise.fr",
     kSafe},
    // caf'e with acute accent' (French)
    {"xn--caf-dma.fr", L"caf\x00e9.fr", kSafe},
    // c-cedillla and a with tilde (Portuguese)
    {"xn--poema-9qae5a.com.br", L"p\x00e3oema\x00e7\x00e3.com.br", kSafe},
    // s with caron
    {"xn--achy-f6a.com",
     L"\x0161"
     L"achy.com",
     kSafe},
    {"xn--kxae4bafwg.gr", L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1.gr",
     kSafe},
    // Eutopia + 123 (Greek)
    {"xn---123-pldm0haj2bk.gr",
     L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1-123.gr", kSafe},
    // Cyrillic (Russian)
    {"xn--n1aeec9b.ru", L"\x0442\x043e\x0440\x0442\x044b.ru", kSafe},
    // Cyrillic + 123 (Russian)
    {"xn---123-45dmmc5f.ru", L"\x0442\x043e\x0440\x0442\x044b-123.ru", kSafe},
    // 'president' in Russian. Is a wholescript confusable, but allowed.
    {"xn--d1abbgf6aiiy.xn--p1ai",
     L"\x043f\x0440\x0435\x0437\x0438\x0434\x0435\x043d\x0442.\x0440\x0444",
     kSafe},
    // Arabic
    {"xn--mgba1fmg.eg", L"\x0627\x0641\x0644\x0627\x0645.eg", kSafe},
    // Hebrew
    {"xn--4dbib.he", L"\x05d5\x05d0\x05d4.he", kSafe},
    // Hebrew + Common
    {"xn---123-ptf2c5c6bt.il", L"\x05e2\x05d1\x05e8\x05d9\x05ea-123.il", kSafe},
    // Thai
    {"xn--12c2cc4ag3b4ccu.th",
     L"\x0e2a\x0e32\x0e22\x0e01\x0e32\x0e23\x0e1a\x0e34\x0e19.th", kSafe},
    // Thai + Common
    {"xn---123-9goxcp8c9db2r.th",
     L"\x0e20\x0e32\x0e29\x0e32\x0e44\x0e17\x0e22-123.th", kSafe},
    // Devangari (Hindi)
    {"www.xn--l1b6a9e1b7c.in", L"www.\x0905\x0915\x094b\x0932\x093e.in", kSafe},
    // Devanagari + Common
    {"xn---123-kbjl2j0bl2k.in", L"\x0939\x093f\x0928\x094d\x0926\x0940-123.in",
     kSafe},

    // Block mixed numeric + numeric lookalike (12.com, using U+0577).
    {"xn--1-9dd.com", L"1߳.com", kUnsafe},

    // Block mixed numeric lookalike + numeric (੨0.com, uses U+0A68).
    {"xn--0-6ee.com", L"੨0.com", kUnsafe},
    // Block fully numeric lookalikes (৪੨.com using U+09EA and U+0A68).
    {"xn--47b6w.com", L"৪੨.com", kUnsafe},
    // Block single script digit lookalikes (using three U+0A68 characters).
    {"xn--qccaa.com", L"੨੨੨.com", kUnsafe},

    // URL test with mostly numbers and one confusable character
    // Georgian 'd' 4000.com
    {"xn--4000-pfr.com",
     L"\x10eb"
     L"4000.com",
     kUnsafe},

    // What used to be 5 Aspirational scripts in the earlier versions of UAX 31.
    // UAX 31 does not define aspirational scripts any more.
    // See http://www.unicode.org/reports/tr31/#Aspirational_Use_Scripts .
    // Unified Canadian Syllabary
    {"xn--dfe0tte.ca", L"\x1456\x14c2\x14ef.ca", kUnsafe},
    // Tifinagh
    {"xn--4ljxa2bb4a6bxb.ma", L"\x2d5c\x2d49\x2d3c\x2d49\x2d4f\x2d30\x2d56.ma",
     kUnsafe},
    // Tifinagh with a disallowed character(U+2D6F)
    {"xn--hmjzaby5d5f.ma", L"\x2d5c\x2d49\x2d3c\x2d6f\x2d49\x2d4f.ma",
     kInvalid},

    // Yi
    {"xn--4o7a6e1x64c.cn", L"\xa188\xa320\xa071\xa0b7.cn", kUnsafe},
    // Mongolian - 'ordu' (place, camp)
    {"xn--56ec8bp.cn", L"\x1823\x1837\x1833\x1824.cn", kUnsafe},
    // Mongolian with a disallowed character
    {"xn--95e5de3ds.cn", L"\x1823\x1837\x1804\x1833\x1824.cn", kUnsafe},
    // Miao/Pollad
    {"xn--2u0fpf0a.cn", L"\U00016f04\U00016f62\U00016f59.cn", kUnsafe},

    // Script mixing tests
    // The following script combinations are allowed.
    // HIGHLY_RESTRICTIVE with Latin limited to ASCII-Latin.
    // ASCII-Latin + Japn (Kana + Han)
    // ASCII-Latin + Kore (Hangul + Han)
    // ASCII-Latin + Han + Bopomofo
    // "payp<alpha>l.com"
    {"xn--paypl-g9d.com", L"payp\x03b1l.com", kUnsafe},
    // google.gr with Greek omicron and epsilon
    {"xn--ggl-6xc1ca.gr", L"g\x03bf\x03bfgl\x03b5.gr", kUnsafe},
    // google.ru with Cyrillic o
    {"xn--ggl-tdd6ba.ru", L"g\x043e\x043egl\x0435.ru", kUnsafe},
    // h<e with acute>llo<China in Han>.cn
    {"xn--hllo-bpa7979ih5m.cn", L"h\x00e9llo\x4e2d\x56fd.cn", kUnsafe},
    // <Greek rho><Cyrillic a><Cyrillic u>.ru
    {"xn--2xa6t2b.ru", L"\x03c1\x0430\x0443.ru", kUnsafe},
    // Georgian + Latin
    {"xn--abcef-vuu.test",
     L"abc\x10eb"
     L"ef.test",
     kUnsafe},
    // Hangul + Latin
    {"xn--han-eb9ll88m.kr", L"\xd55c\xae00han.kr", kSafe},
    // Hangul + Latin + Han with IDN ccTLD
    {"xn--han-or0kq92gkm3c.xn--3e0b707e", L"\xd55c\xae00han\x97d3.\xd55c\xad6d",
     kSafe},
    // non-ASCII Latin + Hangul
    {"xn--caf-dma9024xvpg.kr", L"caf\x00e9\xce74\xd398.kr", kUnsafe},
    // Hangul + Hiragana
    {"xn--y9j3b9855e.kr", L"\xd55c\x3072\x3089.kr", kUnsafe},
    // <Hiragana>.<Hangul> is allowed because script mixing check is per label.
    {"xn--y9j3b.xn--3e0b707e", L"\x3072\x3089.\xd55c\xad6d", kSafe},
    //  Traditional Han + Latin
    {"xn--hanzi-u57ii69i.tw", L"\x6f22\x5b57hanzi.tw", kSafe},
    //  Simplified Han + Latin
    {"xn--hanzi-u57i952h.cn", L"\x6c49\x5b57hanzi.cn", kSafe},
    // Simplified Han + Traditonal Han
    {"xn--hanzi-if9kt8n.cn", L"\x6c49\x6f22hanzi.cn", kSafe},
    //  Han + Hiragana + Katakana + Latin
    {"xn--kanji-ii4dpizfq59yuykqr4b.jp",
     L"\x632f\x308a\x4eee\x540d\x30ab\x30bfkanji.jp", kSafe},
    // Han + Bopomofo
    {"xn--5ekcde0577e87tc.tw", L"\x6ce8\x97f3\x3105\x3106\x3107\x3108.tw",
     kSafe},
    // Han + Latin + Bopomofo
    {"xn--bopo-ty4cghi8509kk7xd.tw",
     L"\x6ce8\x97f3"
     L"bopo\x3105\x3106\x3107\x3108.tw",
     kSafe},
    // Latin + Bopomofo
    {"xn--bopomofo-hj5gkalm.tw", L"bopomofo\x3105\x3106\x3107\x3108.tw", kSafe},
    // Bopomofo + Katakana
    {"xn--lcka3d1bztghi.tw",
     L"\x3105\x3106\x3107\x3108\x30ab\x30bf\x30ab\x30ca.tw", kUnsafe},
    //  Bopomofo + Hangul
    {"xn--5ekcde4543qbec.tw", L"\x3105\x3106\x3107\x3108\xc8fc\xc74c.tw",
     kUnsafe},
    // Devanagari + Latin
    {"xn--ab-3ofh8fqbj6h.in", L"ab\x0939\x093f\x0928\x094d\x0926\x0940.in",
     kUnsafe},
    // Thai + Latin
    {"xn--ab-jsi9al4bxdb6n.th",
     L"ab\x0e20\x0e32\x0e29\x0e32\x0e44\x0e17\x0e22.th", kUnsafe},
    // Armenian + Latin
    {"xn--bs-red.com", L"b\x057ds.com", kUnsafe},
    // Tibetan + Latin
    {"xn--foo-vkm.com", L"foo\x0f37.com", kUnsafe},
    // Oriya + Latin
    {"xn--fo-h3g.com", L"fo\x0b66.com", kUnsafe},
    // Gujarati + Latin
    {"xn--fo-isg.com", L"fo\x0ae6.com", kUnsafe},
    // <vitamin in Katakana>b1.com
    {"xn--b1-xi4a7cvc9f.com",
     L"\x30d3\x30bf\x30df\x30f3"
     L"b1.com",
     kSafe},
    // Devanagari + Han
    {"xn--t2bes3ds6749n.com", L"\x0930\x094b\x0932\x0947\x76e7\x0938.com",
     kUnsafe},
    // Devanagari + Bengali
    {"xn--11b0x.in", L"\x0915\x0995.in", kUnsafe},
    // Canadian Syllabary + Latin
    {"xn--ab-lym.com", L"abᒿ.com", kUnsafe},
    {"xn--ab1-p6q.com", L"ab1ᒿ.com", kUnsafe},
    {"xn--1ab-m6qd.com", L"ᒿ1abᒿ.com", kUnsafe},
    {"xn--ab-jymc.com", L"ᒿabᒿ.com", kUnsafe},
    // Tifinagh + Latin
    {"xn--liy-bq1b.com", L"li\u2d4fy.com", kUnsafe},
    {"xn--rol-cq1b.com", L"rol\u2d4f.com", kUnsafe},
    {"xn--ily-8p1b.com", L"\u2d4fily.com", kUnsafe},
    {"xn--1ly-8p1b.com", L"\u2d4f1ly.com", kUnsafe},

    // Invisibility check
    // Thai tone mark malek(U+0E48) repeated
    {"xn--03c0b3ca.th", L"\x0e23\x0e35\x0e48\x0e48.th", kUnsafe},
    // Accute accent repeated
    {"xn--a-xbba.com", L"a\x0301\x0301.com", kInvalid},
    // 'a' with acuted accent + another acute accent
    {"xn--1ca20i.com", L"\x00e1\x0301.com", kUnsafe},
    // Combining mark at the beginning
    {"xn--abc-fdc.jp", L"\u0300abc.jp", kInvalid},

    // The following three are detected by |dangerous_pattern| regex, but
    // can be regarded as an extension of blocking repeated diacritic marks.
    // i followed by U+0307 (combining dot above)
    {"xn--pixel-8fd.com", L"pi\x0307xel.com", kUnsafe},
    // U+0131 (dotless i) followed by U+0307
    {"xn--pxel-lza43z.com", L"p\x0131\x0307xel.com", kUnsafe},
    // j followed by U+0307 (combining dot above)
    {"xn--jack-qwc.com",
     L"j\x0307"
     L"ack.com",
     kUnsafe},
    // l followed by U+0307
    {"xn--lace-qwc.com",
     L"l\x0307"
     L"ace.com",
     kUnsafe},

    // Do not allow a combining mark after dotless i/j.
    {"xn--pxel-lza29y.com", L"p\x0131\x0300xel.com", kUnsafe},
    {"xn--ack-gpb42h.com",
     L"\x0237\x0301"
     L"ack.com",
     kUnsafe},

    // Mixed script confusable
    // google with Armenian Small Letter Oh(U+0585)
    {"xn--gogle-lkg.com", L"g\x0585ogle.com", kUnsafe},
    {"xn--range-kkg.com", L"\x0585range.com", kUnsafe},
    {"xn--cucko-pkg.com", L"cucko\x0585.com", kUnsafe},
    // Latin 'o' in Armenian.
    {"xn--o-ybcg0cu0cq.com", L"o\x0580\x0574\x0578\x0582\x0566\x0568.com",
     kUnsafe},
    // Hiragana HE(U+3078) mixed with Katakana
    {"xn--49jxi3as0d0fpc.com",
     L"\x30e2\x30d2\x30fc\x30c8\x3078\x30d6\x30f3.com", kUnsafe},

    // U+30FC should be preceded by a Hiragana/Katakana.
    // Katakana + U+30FC + Han
    {"xn--lck0ip02qw5ya.jp", L"\x30ab\x30fc\x91ce\x7403.jp", kSafe},
    // Hiragana + U+30FC + Han
    {"xn--u8j5tr47nw5ya.jp", L"\x304b\x30fc\x91ce\x7403.jp", kSafe},
    // U+30FC + Han
    {"xn--weka801xo02a.com", L"\x30fc\x52d5\x753b\x30fc.com", kUnsafe},
    // Han + U+30FC + Han
    {"xn--wekz60nb2ay85atj0b.jp", L"\x65e5\x672c\x30fc\x91ce\x7403.jp",
     kUnsafe},
    // U+30FC at the beginning
    {"xn--wek060nb2a.jp", L"\x30fc\x65e5\x672c.jp", kUnsafe},
    // Latin + U+30FC + Latin
    {"xn--abcdef-r64e.jp",
     L"abc\x30fc"
     L"def.jp",
     kUnsafe},

    // U+30FB (・) is not allowed next to Latin, but allowed otherwise.
    // U+30FB + Han
    {"xn--vekt920a.jp", L"\x30fb\x91ce.jp", kSafe},
    // Han + U+30FB + Han
    {"xn--vek160nb2ay85atj0b.jp", L"\x65e5\x672c\x30fb\x91ce\x7403.jp", kSafe},
    // Latin + U+30FB + Latin
    {"xn--abcdef-k64e.jp",
     L"abc\x30fb"
     L"def.jp",
     kUnsafe},
    // U+30FB + Latin
    {"xn--abc-os4b.jp",
     L"\x30fb"
     L"abc.jp",
     kUnsafe},

    // U+30FD (ヽ) is allowed only after Katakana.
    // Katakana + U+30FD
    {"xn--lck2i.jp", L"\x30ab\x30fd.jp", kSafe},
    // Hiragana + U+30FD
    {"xn--u8j7t.jp", L"\x304b\x30fd.jp", kUnsafe},
    // Han + U+30FD
    {"xn--xek368f.jp", L"\x4e00\x30fd.jp", kUnsafe},
    {"xn--a-mju.jp", L"a\x30fd.jp", kUnsafe},
    {"xn--a1-bo4a.jp", L"a1\x30fd.jp", kUnsafe},

    // U+30FE (ヾ) is allowed only after Katakana.
    // Katakana + U+30FE
    {"xn--lck4i.jp", L"\x30ab\x30fe.jp", kSafe},
    // Hiragana + U+30FE
    {"xn--u8j9t.jp", L"\x304b\x30fe.jp", kUnsafe},
    // Han + U+30FE
    {"xn--yek168f.jp", L"\x4e00\x30fe.jp", kUnsafe},
    {"xn--a-oju.jp", L"a\x30fe.jp", kUnsafe},
    {"xn--a1-eo4a.jp", L"a1\x30fe.jp", kUnsafe},

    // Cyrillic labels made of Latin-look-alike Cyrillic letters.
    // 1) ѕсоре.com with ѕсоре in Cyrillic.
    {"xn--e1argc3h.com", L"\x0455\x0441\x043e\x0440\x0435.com", kUnsafe},
    // 2) ѕсоре123.com with ѕсоре in Cyrillic.
    {"xn--123-qdd8bmf3n.com",
     L"\x0455\x0441\x043e\x0440\x0435"
     L"123.com",
     kUnsafe},
    // 3) ѕсоре-рау.com with ѕсоре and рау in Cyrillic.
    {"xn----8sbn9akccw8m.com",
     L"\x0455\x0441\x043e\x0440\x0435-\x0440\x0430\x0443.com", kUnsafe},
    // 4) ѕсоре1рау.com with scope and pay in Cyrillic and a non-letter between
    // them.
    {"xn--1-8sbn9akccw8m.com",
     L"\x0455\x0441\x043e\x0440\x0435\x0031\x0440\x0430\x0443.com", kUnsafe},

    // The same as above three, but in IDN TLD (рф).
    // 1) ѕсоре.рф  with ѕсоре in Cyrillic.
    {"xn--e1argc3h.xn--p1ai", L"\x0455\x0441\x043e\x0440\x0435.\x0440\x0444",
     kSafe},
    // 2) ѕсоре123.рф with ѕсоре in Cyrillic.
    {"xn--123-qdd8bmf3n.xn--p1ai",
     L"\x0455\x0441\x043e\x0440\x0435"
     L"123.\x0440\x0444",
     kSafe},
    // 3) ѕсоре-рау.рф with ѕсоре and рау in Cyrillic.
    {"xn----8sbn9akccw8m.xn--p1ai",
     L"\x0455\x0441\x043e\x0440\x0435-\x0440\x0430\x0443.\x0440\x0444", kSafe},
    // 4) ѕсоре1рау.com with scope and pay in Cyrillic and a non-letter between
    // them.
    {"xn--1-8sbn9akccw8m.xn--p1ai",
     L"\x0455\x0441\x043e\x0440\x0435\x0031\x0440\x0430\x0443.\x0440\x0444",
     kSafe},

    // Same as above three, but in .ru TLD.
    // 1) ѕсоре.ru  with ѕсоре in Cyrillic.
    {"xn--e1argc3h.ru", L"\x0455\x0441\x043e\x0440\x0435.ru", kSafe},
    // 2) ѕсоре123.ru with ѕсоре in Cyrillic.
    {"xn--123-qdd8bmf3n.ru",
     L"\x0455\x0441\x043e\x0440\x0435"
     L"123.ru",
     kSafe},
    // 3) ѕсоре-рау.ru with ѕсоре and рау in Cyrillic.
    {"xn----8sbn9akccw8m.ru",
     L"\x0455\x0441\x043e\x0440\x0435-\x0440\x0430\x0443.ru", kSafe},
    // 4) ѕсоре1рау.com with scope and pay in Cyrillic and a non-letter between
    // them.
    {"xn--1-8sbn9akccw8m.ru",
     L"\x0455\x0441\x043e\x0440\x0435\x0031\x0440\x0430\x0443.ru", kSafe},

    // ѕсоре-рау.한국 with ѕсоре and рау in Cyrillic. The label will remain
    // punycode while the TLD will be decoded.
    {"xn----8sbn9akccw8m.xn--3e0b707e", L"xn----8sbn9akccw8m.\xd55c\xad6d",
     kSafe},

    // музей (museum in Russian) has characters without a Latin-look-alike.
    {"xn--e1adhj9a.com", L"\x043c\x0443\x0437\x0435\x0439.com", kSafe},

    // ѕсоԗе.com is Cyrillic with Latin lookalikes.
    {"xn--e1ari3f61c.com", L"\x0455\x0441\x043e\x0517\x0435.com", kUnsafe},

    // ыоԍ.com is Cyrillic with Latin lookalikes.
    {"xn--n1az74c.com", L"\x044b\x043e\x050d.com", kUnsafe},

    // сю.com is Cyrillic with Latin lookalikes.
    {"xn--q1a0a.com", L"\x0441\x044e.com", kUnsafe},

    // Regression test for lowercase letters in whole script confusable
    // lookalike character lists.
    {"xn--80a8a6a.com", L"аьс.com", kUnsafe},

    // googlе.한국 where е is Cyrillic. This tests the generic case when one
    // label is not allowed but  other labels in the domain name are still
    // decoded. Here, googlе is left in punycode but the TLD is decoded.
    {"xn--googl-3we.xn--3e0b707e", L"xn--googl-3we.\xd55c\xad6d", kSafe},

    // Combining Diacritic marks after a script other than Latin-Greek-Cyrillic
    {"xn--rsa2568fvxya.com", L"\xd55c\x0307\xae00.com", kUnsafe},  // 한́글.com
    {"xn--rsa0336bjom.com", L"\x6f22\x0307\x5b57.com", kUnsafe},  // 漢̇字.com
    // नागरी́.com
    {"xn--lsa922apb7a6do.com", L"\x0928\x093e\x0917\x0930\x0940\x0301.com",
     kUnsafe},

    // Similarity checks against the list of top domains. "digklmo68.com" and
    // 'digklmo68.co.uk" are listed for unittest in the top domain list.
    // đigklmo68.com:
    {"xn--igklmo68-kcb.com", L"\x0111igklmo68.com", kUnsafe},
    // www.đigklmo68.com:
    {"www.xn--igklmo68-kcb.com", L"www.\x0111igklmo68.com", kUnsafe},
    // foo.bar.đigklmo68.com:
    {"foo.bar.xn--igklmo68-kcb.com", L"foo.bar.\x0111igklmo68.com", kUnsafe},
    // đigklmo68.co.uk:
    {"xn--igklmo68-kcb.co.uk", L"\x0111igklmo68.co.uk", kUnsafe},
    // mail.đigklmo68.co.uk:
    {"mail.xn--igklmo68-kcb.co.uk", L"mail.\x0111igklmo68.co.uk", kUnsafe},
    // di̇gklmo68.com:
    {"xn--digklmo68-6jf.com", L"di\x0307gklmo68.com", kUnsafe},
    // dig̱klmo68.com:
    {"xn--digklmo68-7vf.com", L"dig\x0331klmo68.com", kUnsafe},
    // digĸlmo68.com:
    {"xn--diglmo68-omb.com", L"dig\x0138lmo68.com", kUnsafe},
    // digkłmo68.com:
    {"xn--digkmo68-9ob.com", L"digk\x0142mo68.com", kUnsafe},
    // digklṃo68.com:
    {"xn--digklo68-l89c.com", L"digkl\x1e43o68.com", kUnsafe},
    // digklmø68.com:
    {"xn--digklm68-b5a.com",
     L"digklm\x00f8"
     L"68.com",
     kUnsafe},
    // digklmoб8.com:
    {"xn--digklmo8-h7g.com",
     L"digklmo\x0431"
     L"8.com",
     kUnsafe},
    // digklmo6৪.com:
    {"xn--digklmo6-7yr.com", L"digklmo6\x09ea.com", kUnsafe},

    // 'islkpx123.com' is in the test domain list.
    // 'іѕӏкрх123' can look like 'islkpx123' in some fonts.
    {"xn--123-bed4a4a6hh40i.com",
     L"\x0456\x0455\x04cf\x043a\x0440\x0445"
     L"123.com",
     kUnsafe},

    // 'o2.com', '28.com', '39.com', '43.com', '89.com', 'oo.com' and 'qq.com'
    // are all explicitly added to the test domain list to aid testing of
    // Latin-lookalikes that are numerics in other character sets and similar
    // edge cases.
    //
    // Bengali:
    {"xn--07be.com", L"\x09e6\x09e8.com", kUnsafe},
    {"xn--27be.com", L"\x09e8\x09ea.com", kUnsafe},
    {"xn--77ba.com", L"\x09ed\x09ed.com", kUnsafe},
    // Gurmukhi:
    {"xn--qcce.com", L"\x0a68\x0a6a.com", kUnsafe},
    {"xn--occe.com", L"\x0a66\x0a68.com", kUnsafe},
    {"xn--rccd.com", L"\x0a6b\x0a69.com", kUnsafe},
    {"xn--pcca.com", L"\x0a67\x0a67.com", kUnsafe},
    // Telugu:
    {"xn--drcb.com", L"\x0c69\x0c68.com", kUnsafe},
    // Devanagari:
    {"xn--d4be.com", L"\x0966\x0968.com", kUnsafe},
    // Kannada:
    {"xn--yucg.com", L"\x0ce6\x0ce9.com", kUnsafe},
    {"xn--yuco.com", L"\x0ce6\x0ced.com", kUnsafe},
    // Oriya:
    {"xn--1jcf.com", L"\x0b6b\x0b68.com", kUnsafe},
    {"xn--zjca.com", L"\x0b66\x0b66.com", kUnsafe},
    // Gujarati:
    {"xn--cgce.com", L"\x0ae6\x0ae8.com", kUnsafe},
    {"xn--fgci.com", L"\x0ae9\x0aed.com", kUnsafe},
    {"xn--dgca.com", L"\x0ae7\x0ae7.com", kUnsafe},

    // wmhtb.com
    {"xn--l1acpvx.com", L"\x0448\x043c\x043d\x0442\x044c.com", kUnsafe},
    // щмнть.com
    {"xn--l1acpzs.com", L"\x0449\x043c\x043d\x0442\x044c.com", kUnsafe},
    // шмнтв.com
    {"xn--b1atdu1a.com", L"\x0448\x043c\x043d\x0442\x0432.com", kUnsafe},
    // шмԋтв.com
    {"xn--b1atsw09g.com", L"\x0448\x043c\x050b\x0442\x0432.com", kUnsafe},
    // шмԧтв.com
    {"xn--b1atsw03i.com", L"\x0448\x043c\x0527\x0442\x0432.com", kUnsafe},
    // шмԋԏв.com
    {"xn--b1at9a12dua.com", L"\x0448\x043c\x050b\x050f\x0432.com", kUnsafe},
    // ഠട345.com
    {"xn--345-jtke.com",
     L"\x0d20\x0d1f"
     L"345.com",
     kUnsafe},

    // Test additional confusable LGC characters (most of them without
    // decomposition into base + diacritc mark). The corresponding ASCII
    // domain names are in the test top domain list.
    // ϼκαωχ.com
    {"xn--mxar4bh6w.com", L"\x03fc\x03ba\x03b1\x03c9\x03c7.com", kUnsafe},
    // þħĸŧƅ.com
    {"xn--vda6f3b2kpf.com", L"\x00fe\x0127\x0138\x0167\x0185.com", kUnsafe},
    // þhktb.com
    {"xn--hktb-9ra.com", L"\x00fehktb.com", kUnsafe},
    // pħktb.com
    {"xn--pktb-5xa.com", L"p\x0127ktb.com", kUnsafe},
    // phĸtb.com
    {"xn--phtb-m0a.com", L"ph\x0138tb.com", kUnsafe},
    // phkŧb.com
    {"xn--phkb-d7a.com",
     L"phk\x0167"
     L"b.com",
     kUnsafe},
    // phktƅ.com
    {"xn--phkt-ocb.com", L"phkt\x0185.com", kUnsafe},
    // ҏнкть.com
    {"xn--j1afq4bxw.com", L"\x048f\x043d\x043a\x0442\x044c.com", kUnsafe},
    // ҏћкть.com
    {"xn--j1aq4a7cvo.com", L"\x048f\x045b\x043a\x0442\x044c.com", kUnsafe},
    // ҏңкть.com
    {"xn--j1aq4azund.com", L"\x048f\x04a3\x043a\x0442\x044c.com", kUnsafe},
    // ҏҥкть.com
    {"xn--j1aq4azuxd.com", L"\x048f\x04a5\x043a\x0442\x044c.com", kUnsafe},
    // ҏӈкть.com
    {"xn--j1aq4azuyj.com", L"\x048f\x04c8\x043a\x0442\x044c.com", kUnsafe},
    // ҏԧкть.com
    {"xn--j1aq4azu9z.com", L"\x048f\x0527\x043a\x0442\x044c.com", kUnsafe},
    // ҏԩкть.com
    {"xn--j1aq4azuq0a.com", L"\x048f\x0529\x043a\x0442\x044c.com", kUnsafe},
    // ҏнқть.com
    {"xn--m1ak4azu6b.com", L"\x048f\x043d\x049b\x0442\x044c.com", kUnsafe},
    // ҏнҝть.com
    {"xn--m1ak4azunc.com", L"\x048f\x043d\x049d\x0442\x044c.com", kUnsafe},
    // ҏнҟть.com
    {"xn--m1ak4azuxc.com", L"\x048f\x043d\x049f\x0442\x044c.com", kUnsafe},
    // ҏнҡть.com
    {"xn--m1ak4azu7c.com", L"\x048f\x043d\x04a1\x0442\x044c.com", kUnsafe},
    // ҏнӄть.com
    {"xn--m1ak4azu8i.com", L"\x048f\x043d\x04c4\x0442\x044c.com", kUnsafe},
    // ҏнԟть.com
    {"xn--m1ak4azuzy.com", L"\x048f\x043d\x051f\x0442\x044c.com", kUnsafe},
    // ҏнԟҭь.com
    {"xn--m1a4a4nnery.com", L"\x048f\x043d\x051f\x04ad\x044c.com", kUnsafe},
    // ҏнԟҭҍ.com
    {"xn--m1a4ne5jry.com", L"\x048f\x043d\x051f\x04ad\x048d.com", kUnsafe},
    // ҏнԟҭв.com
    {"xn--b1av9v8dry.com", L"\x048f\x043d\x051f\x04ad\x0432.com", kUnsafe},
    // ҏӊԟҭв.com
    {"xn--b1a9p8c1e8r.com", L"\x048f\x04ca\x051f\x04ad\x0432.com", kUnsafe},
    // wmŋr.com
    {"xn--wmr-jxa.com", L"wm\x014br.com", kUnsafe},
    // шмпґ.com
    {"xn--l1agz80a.com", L"\x0448\x043c\x043f\x0491.com", kUnsafe},
    // щмпґ.com
    {"xn--l1ag2a0y.com", L"\x0449\x043c\x043f\x0491.com", kUnsafe},
    // щӎпґ.com
    {"xn--o1at1tsi.com", L"\x0449\x04ce\x043f\x0491.com", kUnsafe},
    // ґғ.com
    {"xn--03ae.com", L"\x0491\x0493.com", kUnsafe},
    // ґӻ.com
    {"xn--03a6s.com", L"\x0491\x04fb.com", kUnsafe},
    // ҫұҳҽ.com
    {"xn--r4amg4b.com", L"\x04ab\x04b1\x04b3\x04bd.com", kUnsafe},
    // ҫұӽҽ.com
    {"xn--r4am0b8r.com", L"\x04ab\x04b1\x04fd\x04bd.com", kUnsafe},
    // ҫұӿҽ.com
    {"xn--r4am0b3s.com", L"\x04ab\x04b1\x04ff\x04bd.com", kUnsafe},
    // ҫұӿҿ.com
    {"xn--r4am6b4p.com", L"\x04ab\x04b1\x04ff\x04bf.com", kUnsafe},
    // ҫұӿє.com
    {"xn--91a7osa62a.com", L"\x04ab\x04b1\x04ff\x0454.com", kUnsafe},
    // ӏԃԍ.com
    {"xn--s5a8h4a.com", L"\x04cf\x0503\x050d.com", kUnsafe},

    // U+04CF(ӏ) is mapped to multiple characters, lowercase L(l) and
    // lowercase I(i). Lowercase L is also regarded as similar to digit 1.
    // The test domain list has {ig, ld, 1gd}.com for Cyrillic.
    // ӏԍ.com
    {"xn--s5a8j.com", L"\x04cf\x050d.com", kUnsafe},
    // ӏԃ.com
    {"xn--s5a8h.com", L"\x04cf\x0503.com", kUnsafe},
    // ӏԍԃ.com
    {"xn--s5a8h3a.com", L"\x04cf\x050d\x0503.com", kUnsafe},

    // 1շ34567890.com
    {"xn--134567890-gnk.com", L"1շ34567890.com", kUnsafe},
    // ꓲ2345б7890.com
    {"xn--23457890-e7g93622b.com",
     L"\xa4f2"
     L"2345\x0431"
     L"7890.com",
     kUnsafe},
    // 1ᒿ345б7890.com
    {"xn--13457890-e7g0943b.com",
     L"1\x14bf"
     L"345\x0431"
     L"7890.com",
     kUnsafe},
    // 12з4567890.com
    {"xn--124567890-10h.com",
     L"12\x0437"
     L"4567890.com",
     kUnsafe},
    // 12ҙ4567890.com
    {"xn--124567890-1ti.com",
     L"12\x0499"
     L"4567890.com",
     kUnsafe},
    // 12ӡ4567890.com
    {"xn--124567890-mfj.com",
     L"12\x04e1"
     L"4567890.com",
     kUnsafe},
    // 12उ4567890.com
    {"xn--124567890-m3r.com",
     L"12\u0909"
     L"4567890.com",
     kUnsafe},
    // 12ও4567890.com
    {"xn--124567890-17s.com",
     L"12\u0993"
     L"4567890.com",
     kUnsafe},
    // 12ਤ4567890.com
    {"xn--124567890-hfu.com",
     L"12\u0a24"
     L"4567890.com",
     kUnsafe},
    // 12ဒ4567890.com
    {"xn--124567890-6s6a.com",
     L"12\x1012"
     L"4567890.com",
     kUnsafe},
    // 12ვ4567890.com
    {"xn--124567890-we8a.com",
     L"12\x10D5"
     L"4567890.com",
     kUnsafe},
    // 12პ4567890.com
    {"xn--124567890-hh8a.com",
     L"12\x10DE"
     L"4567890.com",
     kUnsafe},
    // 123ㄐ567890.com
    {"xn--123567890-dr5h.com", L"123ㄐ567890.com", kUnsafe},
    // 123Ꮞ567890.com
    {"xn--123567890-dm4b.com",
     L"123\x13ce"
     L"567890.com",
     kUnsafe},
    // 12345б7890.com
    {"xn--123457890-fzh.com",
     L"12345\x0431"
     L"7890.com",
     kUnsafe},
    // 12345ճ7890.com
    {"xn--123457890-fmk.com", L"12345ճ7890.com", kUnsafe},
    // 1234567ȣ90.com
    {"xn--123456790-6od.com",
     L"1234567\x0223"
     L"90.com",
     kUnsafe},
    // 12345678୨0.com
    {"xn--123456780-71w.com",
     L"12345678\x0b68"
     L"0.com",
     kUnsafe},
    // 123456789ଠ.com
    {"xn--http://123456789-v01b.com", L"http://123456789\x0b20.com", kUnsafe},
    // 123456789ꓳ.com
    {"xn--123456789-tx75a.com", L"123456789\xa4f3.com", kUnsafe},

    // aeœ.com
    {"xn--ae-fsa.com", L"ae\x0153.com", kUnsafe},
    // æce.com
    {"xn--ce-0ia.com",
     L"\x00e6"
     L"ce.com",
     kUnsafe},
    // æœ.com
    {"xn--6ca2t.com", L"\x00e6\x0153.com", kUnsafe},
    // ӕԥ.com
    {"xn--y5a4n.com", L"\x04d5\x0525.com", kUnsafe},

    // ငၔဌ၂ဝ.com (entirely made of Myanmar characters)
    {"xn--ridq5c9hnd.com",
     L"\x1004\x1054\x100c"
     L"\x1042\x101d.com",
     kUnsafe},

    // ฟรฟร.com (made of two Thai characters. similar to wsws.com in
    // some fonts)
    {"xn--w3calb.com", L"\x0e1f\x0e23\x0e1f\x0e23.com", kUnsafe},
    // พรบ.com
    {"xn--r3chp.com", L"\x0e1e\x0e23\x0e1a.com", kUnsafe},
    // ฟรบ.com
    {"xn--r3cjm.com", L"\x0e1f\x0e23\x0e1a.com", kUnsafe},

    // Lao characters that look like w, s, o, and u.
    // ພຣບ.com
    {"xn--f7chp.com", L"\x0e9e\x0ea3\x0e9a.com", kUnsafe},
    // ຟຣບ.com
    {"xn--f7cjm.com", L"\x0e9f\x0ea3\x0e9a.com", kUnsafe},
    // ຟຮບ.com
    {"xn--f7cj9b.com", L"\x0e9f\x0eae\x0e9a.com", kUnsafe},
    // ຟຮ໐ບ.com
    {"xn--f7cj9b5h.com",
     L"\x0e9f\x0eae"
     L"\x0ed0\x0e9a.com",
     kUnsafe},

    // Lao character that looks like n.
    // ก11.com
    {"xn--11-lqi.com",
     L"\x0e01"
     L"11.com",
     kUnsafe},

    // At one point the skeleton of 'w' was 'vv', ensure that
    // that it's treated as 'w'.
    {"xn--wder-qqa.com",
     L"w\x00f3"
     L"der.com",
     kUnsafe},

    // Mixed digits: the first two will also fail mixed script test
    // Latin + ASCII digit + Deva digit
    {"xn--asc1deva-j0q.co.in", L"asc1deva\x0967.co.in", kUnsafe},
    // Latin + Deva digit + Beng digit
    {"xn--devabeng-f0qu3f.co.in",
     L"deva\x0967"
     L"beng\x09e7.co.in",
     kUnsafe},
    // ASCII digit + Deva digit
    {"xn--79-v5f.co.in",
     L"7\x09ea"
     L"9.co.in",
     kUnsafe},
    //  Deva digit + Beng digit
    {"xn--e4b0x.co.in", L"\x0967\x09e7.co.in", kUnsafe},
    // U+4E00 (CJK Ideograph One) is not a digit, but it's not allowed next to
    // non-Kana scripts including numbers.
    {"xn--d12-s18d.cn", L"d12\x4e00.cn", kUnsafe},
    // One that's really long that will force a buffer realloc
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaa",
     L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     L"aaaaaaaa",
     kSafe},

    // Not allowed; characters outside [:Identifier_Status=Allowed:]
    // Limited Use Scripts: UTS 31 Table 7.
    // Vai
    {"xn--sn8a.com", L"\xa50b.com", kUnsafe},
    // 'CARD' look-alike in Cherokee
    {"xn--58db0a9q.com", L"\x13df\x13aa\x13a1\x13a0.com", kUnsafe},
    // Scripts excluded from Identifiers: UTS 31 Table 4
    // Coptic
    {"xn--5ya.com", L"\x03e7.com", kUnsafe},
    // Old Italic
    {"xn--097cc.com", L"\U00010300\U00010301.com", kUnsafe},

    // U+115F (Hangul Filler)
    {"xn--osd3820f24c.kr", L"\xac00\xb098\x115f.kr", kInvalid},
    {"www.xn--google-ho0coa.com", L"www.\x2039google\x203a.com", kUnsafe},
    // Latin small capital w: hardᴡare.com
    {"xn--hardare-l41c.com",
     L"hard\x1d21"
     L"are.com",
     kUnsafe},
    // Minus Sign(U+2212)
    {"xn--t9g238xc2a.jp", L"\x65e5\x2212\x672c.jp", kUnsafe},
    // Latin Small Letter Script G: ɡɡ.com
    {"xn--0naa.com", L"\x0261\x0261.com", kUnsafe},
    // Hangul Jamo(U+11xx)
    {"xn--0pdc3b.com", L"\x1102\x1103\x1110.com", kUnsafe},
    // degree sign: 36°c.com
    {"xn--36c-tfa.com",
     L"36\x00b0"
     L"c.com",
     kUnsafe},
    // Pound sign
    {"xn--5free-fga.com", L"5free\x00a3.com", kUnsafe},
    // Hebrew points (U+05B0, U+05B6)
    {"xn--7cbl2kc2a.com", L"\x05e1\x05b6\x05e7\x05b0\x05e1.com", kUnsafe},
    // Danda(U+0964)
    {"xn--81bp1b6ch8s.com", L"\x0924\x093f\x091c\x0964\x0930\x0940.com",
     kUnsafe},
    // Small letter script G(U+0261)
    {"xn--oogle-qmc.com", L"\x0261oogle.com", kUnsafe},
    // Small Katakana Extension(U+31F1)
    {"xn--wlk.com", L"\x31f1.com", kUnsafe},
    // Heart symbol: ♥
    {"xn--ab-u0x.com", L"ab\x2665.com", kUnsafe},
    // Emoji
    {"xn--vi8hiv.xyz", L"\U0001f355\U0001f4a9.xyz", kUnsafe},
    // Registered trade mark
    {"xn--egistered-fna.com",
     L"\x00ae"
     L"egistered.com",
     kUnsafe},
    // Latin Letter Retroflex Click
    {"xn--registered-25c.com", L"registered\x01c3.com", kUnsafe},
    // ASCII '!' not allowed in IDN
    {"xn--!-257eu42c.kr", L"\xc548\xb155!.kr", kUnsafe},
    // 'GOOGLE' in IPA extension: ɢᴏᴏɢʟᴇ
    {"xn--1naa7pn51hcbaa.com", L"\x0262\x1d0f\x1d0f\x0262\x029f\x1d07.com",
     kUnsafe},
    // Padlock icon spoof.
    {"xn--google-hj64e.com", L"\U0001f512google.com", kUnsafe},

    // Custom black list
    // Combining Long Solidus Overlay
    {"google.xn--comabc-k8d",
     L"google.com\x0338"
     L"abc",
     kUnsafe},
    // Hyphenation Point instead of Katakana Middle dot
    {"xn--svgy16dha.jp", L"\x30a1\x2027\x30a3.jp", kUnsafe},
    // Gershayim with other Hebrew characters is allowed.
    {"xn--5db6bh9b.il", L"\x05e9\x05d1\x05f4\x05e6.il", kSafe},
    // Hebrew Gershayim with Latin is invalid according to Python's idna
    // package.
    {"xn--ab-yod.com",
     L"a\x05f4"
     L"b.com",
     kInvalid},
    // Hebrew Gershayim with Arabic is disallowed.
    {"xn--5eb7h.eg", L"\x0628\x05f4.eg", kUnsafe},
#if defined(OS_APPLE)
    // These characters are blocked due to a font issue on Mac.
    // Tibetan transliteration characters.
    {"xn--com-lum.test.pl", L"com\u0f8c.test.pl", kUnsafe},
    // Arabic letter KASHMIRI YEH
    {"xn--fgb.com", L"\u0620.com", kUnsafe},
#endif

    // Hyphens (http://unicode.org/cldr/utility/confusables.jsp?a=-)
    // Hyphen-Minus (the only hyphen allowed)
    // abc-def
    {"abc-def.com", L"abc-def.com", kSafe},
    // Modifier Letter Minus Sign
    {"xn--abcdef-5od.com",
     L"abc\x02d7"
     L"def.com",
     kUnsafe},
    // Hyphen
    {"xn--abcdef-dg0c.com",
     L"abc\x2010"
     L"def.com",
     kUnsafe},
    // Non-Breaking Hyphen
    // This is actually an invalid IDNA domain (U+2011 normalizes to U+2010),
    // but it is included to ensure that we do not inadvertently allow this
    // character to be displayed as Unicode.
    {"xn--abcdef-kg0c.com",
     L"abc\x2011"
     L"def.com",
     kInvalid},
    // Figure Dash.
    // Python's idna package refuses to decode the minus signs and dashes. ICU
    // decodes them but treats them as unsafe in spoof checks, so these test
    // cases are marked as unsafe instead of invalid.
    {"xn--abcdef-rg0c.com",
     L"abc\x2012"
     L"def.com",
     kUnsafe},
    // En Dash
    {"xn--abcdef-yg0c.com",
     L"abc\x2013"
     L"def.com",
     kUnsafe},
    // Hyphen Bullet
    {"xn--abcdef-kq0c.com",
     L"abc\x2043"
     L"def.com",
     kUnsafe},
    // Minus Sign
    {"xn--abcdef-5d3c.com",
     L"abc\x2212"
     L"def.com",
     kUnsafe},
    // Heavy Minus Sign
    {"xn--abcdef-kg1d.com",
     L"abc\x2796"
     L"def.com",
     kUnsafe},
    // Em Dash
    // Small Em Dash (U+FE58) is normalized to Em Dash.
    {"xn--abcdef-5g0c.com",
     L"abc\x2014"
     L"def.com",
     kUnsafe},
    // Coptic Small Letter Dialect-P Ni. Looks like dash.
    // Coptic Capital Letter Dialect-P Ni is normalized to small letter.
    {"xn--abcdef-yy8d.com",
     L"abc\x2cbb"
     L"def.com",
     kUnsafe},

    // Block NV8 (Not valid in IDN 2008) characters.
    // U+058A (֊)
    {"xn--ab-vfd.com",
     L"a\x058a"
     L"b.com",
     kUnsafe},
    {"xn--y9ac3j.com", L"\x0561\x058a\x0562.com", kUnsafe},
    // U+2019 (’)
    {"xn--ab-n2t.com",
     L"a\x2019"
     L"b.com",
     kUnsafe},
    // U+2027 (‧)
    {"xn--ab-u3t.com",
     L"a\x2027"
     L"b.com",
     kUnsafe},
    // U+30A0 (゠)
    {"xn--ab-bg4a.com",
     L"a\x30a0"
     L"b.com",
     kUnsafe},
    {"xn--9bk3828aea.com", L"\xac00\x30a0\xac01.com", kUnsafe},
    {"xn--9bk279fba.com", L"\x4e00\x30a0\x4e00.com", kUnsafe},
    {"xn--n8jl2x.com", L"\x304a\x30a0\x3044.com", kUnsafe},
    {"xn--fbke7f.com", L"\x3082\x30a0\x3084.com", kUnsafe},

    // Block single/double-quote-like characters.
    // U+02BB (ʻ)
    {"xn--ab-8nb.com",
     L"a\x02bb"
     L"b.com",
     kUnsafe},
    // U+02BC (ʼ)
    {"xn--ab-cob.com",
     L"a\x02bc"
     L"b.com",
     kUnsafe},
    // U+144A: Not allowed to mix with scripts other than Canadian Syllabics.
    {"xn--ab-jom.com",
     L"a\x144a"
     L"b.com",
     kUnsafe},
    {"xn--xcec9s.com", L"\x1401\x144a\x1402.com", kUnsafe},

    // Custom dangerous patterns
    // Two Katakana-Hiragana combining mark in a row
    {"google.xn--com-oh4ba.evil.jp", L"google.com\x309a\x309a.evil.jp",
     kUnsafe},
    // Katakana Letter No not enclosed by {Han,Hiragana,Katakana}.
    {"google.xn--comevil-v04f.jp",
     L"google.com\x30ce"
     L"evil.jp",
     kUnsafe},
    // TODO(jshin): Review the danger of allowing the following two.
    // Hiragana 'No' by itself is allowed.
    {"xn--ldk.jp", L"\x30ce.jp", kSafe},
    // Hebrew Gershayim used by itself is allowed.
    {"xn--5eb.il", L"\x05f4.il", kSafe},

    // Block RTL nonspacing marks (NSM) after unrelated scripts.
    {"xn--foog-ycg.com", L"foog\x0650.com", kUnsafe},    // Latin + Arabic NSM
    {"xn--foog-jdg.com", L"foog\x0654.com", kUnsafe},    // Latin + Arabic NSM
    {"xn--foog-jhg.com", L"foog\x0670.com", kUnsafe},    // Latin + Arbic NSM
    {"xn--foog-opf.com", L"foog\x05b4.com", kUnsafe},    // Latin + Hebrew NSM
    {"xn--shb5495f.com", L"\xac00\x0650.com", kUnsafe},  // Hang + Arabic NSM

    // 4 Deviation characters between IDNA 2003 and IDNA 2008
    // When entered in Unicode, the first two are mapped to 'ss' and Greek sigma
    // and the latter two are mapped away. However, the punycode form should
    // remain in punycode.
    // U+00DF(sharp-s)
    {"xn--fu-hia.de", L"fu\x00df.de", kUnsafe},
    // U+03C2(final-sigma)
    {"xn--mxac2c.gr", L"\x03b1\x03b2\x03c2.gr", kUnsafe},
    // U+200C(ZWNJ)
    {"xn--h2by8byc123p.in", L"\x0924\x094d\x200c\x0930\x093f.in", kUnsafe},
    // U+200C(ZWJ)
    {"xn--11b6iy14e.in", L"\x0915\x094d\x200d.in", kUnsafe},

    // Math Monospace Small A. When entered in Unicode, it's canonicalized to
    // 'a'. The punycode form should remain in punycode.
    {"xn--bc-9x80a.xyz",
     L"\U0001d68a"
     L"bc.xyz",
     kInvalid},
    // Math Sans Bold Capital Alpha
    {"xn--bc-rg90a.xyz",
     L"\U0001d756"
     L"bc.xyz",
     kInvalid},
    // U+3000 is canonicalized to a space(U+0020), but the punycode form
    // should remain in punycode.
    {"xn--p6j412gn7f.cn", L"\x4e2d\x56fd\x3000", kInvalid},
    // U+3002 is canonicalized to ASCII fullstop(U+002E), but the punycode form
    // should remain in punycode.
    {"xn--r6j012gn7f.cn", L"\x4e2d\x56fd\x3002", kInvalid},
    // Invalid punycode
    // Has a codepoint beyond U+10FFFF.
    {"xn--krank-kg706554a", nullptr, kInvalid},
    // '?' in punycode.
    {"xn--hello?world.com", nullptr, kInvalid},

    // Not allowed in UTS46/IDNA 2008
    // Georgian Capital Letter(U+10BD)
    {"xn--1nd.com", L"\x10bd.com", kInvalid},
    // 3rd and 4th characters are '-'.
    {"xn-----8kci4dhsd", L"\x0440\x0443--\x0430\x0432\x0442\x043e", kInvalid},
    // Leading combining mark
    {"xn--72b.com", L"\x093e.com", kInvalid},
    // BiDi check per IDNA 2008/UTS 46
    // Cannot starts with AN(Arabic-Indic Number)
    {"xn--8hbae.eg", L"\x0662\x0660\x0660.eg", kInvalid},
    // Cannot start with a RTL character and ends with a LTR
    {"xn--x-ymcov.eg", L"\x062c\x0627\x0631x.eg", kInvalid},
    // Can start with a RTL character and ends with EN(European Number)
    {"xn--2-ymcov.eg",
     L"\x062c\x0627\x0631"
     L"2.eg",
     kSafe},
    // Can start with a RTL and end with AN
    {"xn--mgbjq0r.eg", L"\x062c\x0627\x0631\x0662.eg", kSafe},

    // Extremely rare Latin letters
    // Latin Ext B - Pinyin: ǔnion.com
    {"xn--nion-unb.com", L"\x01d4nion.com", kUnsafe},
    // Latin Ext C: ⱴase.com
    {"xn--ase-7z0b.com",
     L"\x2c74"
     L"ase.com",
     kUnsafe},
    // Latin Ext D: ꝴode.com
    {"xn--ode-ut3l.com", L"\xa774ode.com", kUnsafe},
    // Latin Ext Additional: ḷily.com
    {"xn--ily-n3y.com", L"\x1e37ily.com", kUnsafe},
    // Latin Ext E: ꬺove.com
    {"xn--ove-8y6l.com", L"\xab3aove.com", kUnsafe},
    // Greek Ext: ᾳβγ.com
    {"xn--nxac616s.com", L"\x1fb3\x03b2\x03b3.com", kInvalid},
    // Cyrillic Ext A (label cannot begin with an illegal combining character).
    {"xn--lrj.com", L"\x2def.com", kInvalid},
    // Cyrillic Ext B: ꙡ.com
    {"xn--kx8a.com", L"\xa661.com", kUnsafe},
    // Cyrillic Ext C: ᲂ.com (Narrow o)
    {"xn--43f.com", L"\x1c82.com", kInvalid},

    // The skeleton of Extended Arabic-Indic Digit Zero (۰) is a dot. Check that
    // this is handled correctly (crbug/877045).
    {"xn--dmb", L"\x06f0", kSafe},

    // Test that top domains whose skeletons are the same as the domain name are
    // handled properly. In this case, tést.net should match test.net top
    // domain and not be converted to unicode.
    {"xn--tst-bma.net", L"t\x00e9st.net", kUnsafe},
    // Variations of the above, for testing crbug.com/925199.
    // some.tést.net should match test.net.
    {"some.xn--tst-bma.net", L"some.t\x00e9st.net", kUnsafe},
    // The following should not match test.net, so should be converted to
    // unicode.
    // ést.net (a suffix of tést.net).
    {"xn--st-9ia.net", L"\x00e9st.net", kSafe},
    // some.ést.net
    {"some.xn--st-9ia.net", L"some.\x00e9st.net", kSafe},
    // atést.net (tést.net is a suffix of atést.net)
    {"xn--atst-cpa.net", L"at\x00e9st.net", kSafe},
    // some.atést.net
    {"some.xn--atst-cpa.net", L"some.at\x00e9st.net", kSafe},

    // Modifier-letter-voicing should be blocked (wwwˬtest.com).
    {"xn--wwwtest-2be.com", L"www\x02ectest.com", kUnsafe},

    // oĸ.com: Not a top domain, should be blocked because of Kra.
    {"xn--o-tka.com", L"o\x0138.com", kUnsafe},

    // U+4E00 and U+3127 should be blocked when next to non-CJK.
    {"xn--ipaddress-w75n.com", L"ip一address.com", kUnsafe},
    {"xn--ipaddress-wx5h.com", L"ipㄧaddress.com", kUnsafe},
    // U+4E00 at the beginning and end of a string.
    {"xn--google-gg5e.com", L"googleㄧ.com", kUnsafe},
    {"xn--google-9f5e.com", L"ㄧgoogle.com", kUnsafe},
    // These are allowed because 一 is not immediately next to non-CJK.
    {"xn--gamer-fg1hz05u.com", L"一生gamer.com", kSafe},
    {"xn--gamer-kg1hy05u.com", L"gamer生一.com", kSafe},
    {"xn--4gqz91g.com", L"一猫.com", kSafe},
    {"xn--4fkv10r.com", L"ㄧ猫.com", kSafe},
    // U+4E00 with another ideograph.
    {"xn--4gqc.com", L"一丁.com", kSafe},

    // CJK ideographs looking like slashes should be blocked when next to
    // non-CJK.
    {"example.xn--comtest-k63k", L"example.com丶test", kUnsafe},
    {"example.xn--comtest-u83k", L"example.com乀test", kUnsafe},
    {"example.xn--comtest-283k", L"example.com乁test", kUnsafe},
    {"example.xn--comtest-m83k", L"example.com丿test", kUnsafe},
    // This is allowed because the ideographs are not immediately next to
    // non-CJK.
    {"xn--oiqsace.com", L"丶乀乁丿.com", kSafe},

    // Kana voiced sound marks are not allowed.
    {"xn--google-1m4e.com", L"google\x3099.com", kUnsafe},
    {"xn--google-8m4e.com", L"google\x309A.com", kUnsafe},

    // Small letter theta looks like a zero.
    {"xn--123456789-yzg.com", L"123456789θ.com", kUnsafe},

    {"xn--est-118d.net", L"七est.net", kUnsafe},
    {"xn--est-918d.net", L"丅est.net", kUnsafe},
    {"xn--est-e28d.net", L"丆est.net", kUnsafe},
    {"xn--est-t18d.net", L"丁est.net", kUnsafe},
    {"xn--3-cq6a.com", L"丩3.com", kUnsafe},
    {"xn--cxe-n68d.com", L"c丫xe.com", kUnsafe},
    {"xn--cye-b98d.com", L"cy乂e.com", kUnsafe},

    // U+05D7 can look like Latin n in many fonts.
    {"xn--ceba.com", L"חח.com", kUnsafe},

    // U+00FE (þ) and U+00F0 (ð) are only allowed under the .is TLD.
    {"xn--acdef-wva.com", L"aþcdef.com", kUnsafe},
    {"xn--mnpqr-jta.com", L"mnðpqr.com", kUnsafe},
    {"xn--acdef-wva.is", L"aþcdef.is", kSafe},
    {"xn--mnpqr-jta.is", L"mnðpqr.is", kSafe},

    // U+0259 (ə) is only allowed under the .az TLD.
    {"xn--xample-vyc.com", L"əxample.com", kUnsafe},
    {"xn--xample-vyc.az", L"əxample.az", kSafe},

    // U+00B7 is only allowed on Catalan domains between two l's.
    {"xn--googlecom-5pa.com", L"google·com.com", kUnsafe},
    {"xn--ll-0ea.com", L"l·l.com", kUnsafe},
    {"xn--ll-0ea.cat", L"l·l.cat", kSafe},
    {"xn--al-0ea.cat", L"a·l.cat", kUnsafe},
    {"xn--la-0ea.cat", L"l·a.cat", kUnsafe},
    {"xn--l-fda.cat", L"·l.cat", kUnsafe},
    {"xn--l-gda.cat", L"l·.cat", kUnsafe},

    {"xn--googlecom-gk6n.com", L"google丨com.com", kUnsafe},   // (U+4E28)
    {"xn--googlecom-0y6n.com", L"google乛com.com", kUnsafe},   // (U+4E5B)
    {"xn--googlecom-v85n.com", L"google七com.com", kUnsafe},   // (U+4E03)
    {"xn--googlecom-g95n.com", L"google丅com.com", kUnsafe},   // (U+4E05)
    {"xn--googlecom-go6n.com", L"google丶com.com", kUnsafe},   // (U+4E36)
    {"xn--googlecom-b76o.com", L"google十com.com", kUnsafe},   // (U+5341)
    {"xn--googlecom-ql3h.com", L"google〇com.com", kUnsafe},   // (U+3007)
    {"xn--googlecom-0r5h.com", L"googleㄒcom.com", kUnsafe},   // (U+3112)
    {"xn--googlecom-bu5h.com", L"googleㄚcom.com", kUnsafe},   // (U+311A)
    {"xn--googlecom-qv5h.com", L"googleㄟcom.com", kUnsafe},   // (U+311F)
    {"xn--googlecom-0x5h.com", L"googleㄧcom.com", kUnsafe},   // (U+3127)
    {"xn--googlecom-by5h.com", L"googleㄨcom.com", kUnsafe},   // (U+3128)
    {"xn--googlecom-ly5h.com", L"googleㄩcom.com", kUnsafe},   // (U+3129)
    {"xn--googlecom-5o5h.com", L"googleㄈcom.com", kUnsafe},   // (U+3108)
    {"xn--googlecom-075n.com", L"google一com.com", kUnsafe},   // (U+4E00)
    {"xn--googlecom-046h.com", L"googleㆺcom.com", kUnsafe},   // (U+31BA)
    {"xn--googlecom-026h.com", L"googleㆳcom.com", kUnsafe},   // (U+31B3)
    {"xn--googlecom-lg9q.com", L"google工com.com", kUnsafe},   // (U+5DE5)
    {"xn--googlecom-g040a.com", L"google讠com.com", kUnsafe},  // (U+8BA0)
    {"xn--googlecom-b85n.com", L"google丁com.com", kUnsafe},   // (U+4E01)

    // Whole-script-confusables. Cyrillic is sufficiently handled in cases above
    // so it's not included here.
    // Armenian:
    {"xn--mbbkpm.com", L"ոսւօ.com", kUnsafe},
    {"xn--mbbkpm.am", L"ոսւօ.am", kSafe},
    {"xn--mbbkpm.xn--y9a3aq", L"ոսւօ.հայ", kSafe},
    // Ethiopic:
    {"xn--6xd66aa62c.com", L"ሠዐዐፐ.com", kUnsafe},
    {"xn--6xd66aa62c.et", L"ሠዐዐፐ.et", kSafe},
    {"xn--6xd66aa62c.xn--m0d3gwjla96a", L"ሠዐዐፐ.ኢትዮጵያ", kSafe},
    // Greek:
    {"xn--mxapd.com", L"ικα.com", kUnsafe},
    {"xn--mxapd.gr", L"ικα.gr", kSafe},
    {"xn--mxapd.xn--qxam", L"ικα.ελ", kSafe},
    // Georgian:
    {"xn--gpd3ag.com", L"ჽჿხ.com", kUnsafe},
    {"xn--gpd3ag.ge", L"ჽჿხ.ge", kSafe},
    {"xn--gpd3ag.xn--node", L"ჽჿხ.გე", kSafe},
    // Hebrew:
    {"xn--7dbh4a.com", L"חסד.com", kUnsafe},
    {"xn--7dbh4a.il", L"חסד.il", kSafe},
    {"xn--9dbq2a.xn--7dbh4a", L"קום.חסד", kSafe},
    // Myanmar:
    {"xn--oidbbf41a.com", L"င၀ဂခဂ.com", kUnsafe},
    {"xn--oidbbf41a.mm", L"င၀ဂခဂ.mm", kSafe},
    {"xn--oidbbf41a.xn--7idjb0f4ck", L"င၀ဂခဂ.မြန်မာ", kSafe},
    // Myanmar Shan digits:
    {"xn--rmdcmef.com", L"႐႑႕႖႗.com", kUnsafe},
    {"xn--rmdcmef.mm", L"႐႑႕႖႗.mm", kSafe},
    {"xn--rmdcmef.xn--7idjb0f4ck", L"႐႑႕႖႗.မြန်မာ", kSafe},
// Thai:
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    {"xn--o3cedqz2c.com", L"ทนบพรห.com", kUnsafe},
    {"xn--o3cedqz2c.th", L"ทนบพรห.th", kSafe},
    {"xn--o3cedqz2c.xn--o3cw4h", L"ทนบพรห.ไทย", kSafe},
#else
    {"xn--r3ch7hsc.com", L"พบเ๐.com", kUnsafe},
    {"xn--r3ch7hsc.th", L"พบเ๐.th", kSafe},
    {"xn--r3ch7hsc.xn--o3cw4h", L"พบเ๐.ไทย", kSafe},
#endif

    // Indic scripts:
    // Bengali:
    {"xn--07baub.com", L"০৭০৭.com", kUnsafe},
    // Devanagari:
    {"xn--62ba6j.com", L"ऽ०ऽ.com", kUnsafe},
    // Gujarati:
    {"xn--becd.com", L"ડટ.com", kUnsafe},
    // Gurmukhi:
    {"xn--occacb.com", L"੦੧੦੧.com", kUnsafe},
    // Kannada:
    {"xn--stca6jf.com", L"ಽ೦ಽ೧.com", kUnsafe},
    // Malayalam:
    {"xn--lwccv.com", L"ടഠധ.com", kUnsafe},
    // Oriya:
    {"xn--zhca6ub.com", L"୮ଠ୮ଠ.com", kUnsafe},
    // Tamil:
    {"xn--mlca6ab.com", L"டபடப.com", kUnsafe},
    // Telugu:
    {"xn--brcaabbb.com", L"౧౦౧౦౧౦.com", kUnsafe},

    // IDN domain matching an IDN top-domain (fóó.com)
    {"xn--fo-5ja.com", L"fóo.com", kUnsafe},

    // crbug.com/769547: Subdomains of top domains should be allowed.
    {"xn--xample-9ua.test.net", L"éxample.test.net", kSafe},
    // Skeleton of the eTLD+1 matches a top domain, but the eTLD+1 itself is
    // not a top domain. Should not be decoded to unicode.
    {"xn--xample-9ua.test.xn--nt-bja", L"éxample.test.nét", kUnsafe},
};

namespace test {
#include "components/url_formatter/spoof_checks/top_domains/test_domains-trie-inc.cc"
}

bool IsPunycode(const base::string16& s) {
  return s.size() > 4 && s[0] == L'x' && s[1] == L'n' && s[2] == L'-' &&
         s[3] == L'-';
}

}  // namespace

class IDNSpoofCheckerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IDNSpoofChecker::HuffmanTrieParams trie_params{
        test::kTopDomainsHuffmanTree, sizeof(test::kTopDomainsHuffmanTree),
        test::kTopDomainsTrie, test::kTopDomainsTrieBits,
        test::kTopDomainsRootPosition};
    IDNSpoofChecker::SetTrieParamsForTesting(trie_params);
  }

  void TearDown() override { IDNSpoofChecker::RestoreTrieParamsForTesting(); }
};

// Test that a domain entered as punycode is decoded to unicode if safe,
// otherwise is left in punycode.
//
// TODO(crbug.com/1036523): This should also check if a domain entered as
// unicode is properly decoded or not-decoded. This is important in cases where
// certain unicode characters are canonicalized to other characters.
// E.g. Mathematical Monospace Small A (U+1D68A) is canonicalized to "a" when
// used in a domain name.
TEST_F(IDNSpoofCheckerTest, IDNToUnicode) {
  for (size_t i = 0; i < base::size(kIdnCases); i++) {
    // Sanity check to ensure that the unicode output matches the input. Bypass
    // all spoof checks by doing an unsafe conversion.
    const IDNConversionResult unsafe_result =
        UnsafeIDNToUnicodeWithDetails(kIdnCases[i].input);

    // Ignore inputs that can't be converted even with unsafe conversion because
    // they contain certain characters not allowed in IDNs. E.g. U+24df (Latin
    // CIRCLED LATIN SMALL LETTER P) in hostname causes the conversion to fail
    // before reaching spoof checks.
    if (kIdnCases[i].expected_result != kInvalid) {
      // Also ignore domains that need to remain partially in punycode, such
      // as ѕсоре-рау.한국 where scope-pay is a Cyrillic whole-script
      // confusable but 한국 is safe. This would require adding yet another
      // field to the the test struct.
      if (!IsPunycode(WideToUTF16(kIdnCases[i].unicode_output))) {
        ASSERT_EQ(unsafe_result.result,
                  WideToUTF16(kIdnCases[i].unicode_output));
      }
    } else {
      // Invalid punycode should not be converted.
      ASSERT_EQ(unsafe_result.result, ASCIIToUTF16(kIdnCases[i].input));
    }

    const base::string16 output(IDNToUnicode(kIdnCases[i].input));
    const base::string16 expected(kIdnCases[i].expected_result == kSafe
                                      ? WideToUTF16(kIdnCases[i].unicode_output)
                                      : ASCIIToUTF16(kIdnCases[i].input));
    EXPECT_EQ(expected, output)
        << "input # " << i << ": \"" << kIdnCases[i].input << "\"";
  }
}

TEST_F(IDNSpoofCheckerTest, GetSimilarTopDomain) {
  struct TestCase {
    const wchar_t* const hostname;
    const char* const expected_top_domain;
  } kTestCases[] = {
      {L"tést.net", "test.net"},
      {L"subdomain.tést.net", "test.net"},
      // A top domain should not return a similar top domain result.
      {L"test.net", ""},
      // A subdomain of a top domain should not return a similar top domain
      // result.
      {L"subdomain.test.net", ""},
      // An IDN subdomain of a top domain should not return a similar top domain
      // result.
      {L"subdómain.test.net", ""}};
  for (const TestCase& test_case : kTestCases) {
    const TopDomainEntry entry = IDNSpoofChecker().GetSimilarTopDomain(
        base::WideToUTF16(test_case.hostname));
    EXPECT_EQ(test_case.expected_top_domain, entry.domain);
    EXPECT_FALSE(entry.is_top_500);
  }
}

TEST_F(IDNSpoofCheckerTest, LookupSkeletonInTopDomains) {
  {
    TopDomainEntry entry =
        IDNSpoofChecker().LookupSkeletonInTopDomains("d4OOO.corn");
    EXPECT_EQ("d4000.com", entry.domain);
    EXPECT_TRUE(entry.is_top_500);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kFull);
  }
  {
    TopDomainEntry entry = IDNSpoofChecker().LookupSkeletonInTopDomains(
        "d4OOOcorn", SkeletonType::kSeparatorsRemoved);
    EXPECT_EQ("d4000.com", entry.domain);
    EXPECT_TRUE(entry.is_top_500);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kSeparatorsRemoved);
  }
  {
    TopDomainEntry entry =
        IDNSpoofChecker().LookupSkeletonInTopDomains("digklrno68.corn");
    EXPECT_EQ("digklmo68.com", entry.domain);
    EXPECT_FALSE(entry.is_top_500);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kFull);
  }
}

// Same test as LookupSkeletonInTopDomains but using the real top domain list.
TEST(IDNSpoofCheckerNoFixtureTest, LookupSkeletonInTopDomains) {
  {
    TopDomainEntry entry =
        IDNSpoofChecker().LookupSkeletonInTopDomains("google.corn");
    EXPECT_EQ("google.com", entry.domain);
    EXPECT_TRUE(entry.is_top_500);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kFull);
  }
  {
    TopDomainEntry entry = IDNSpoofChecker().LookupSkeletonInTopDomains(
        "googlecorn", SkeletonType::kSeparatorsRemoved);
    EXPECT_EQ("google.com", entry.domain);
    EXPECT_TRUE(entry.is_top_500);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kSeparatorsRemoved);
  }
  {
    // This is data dependent, must be updated when the top domain list
    // is updated.
    TopDomainEntry entry =
        IDNSpoofChecker().LookupSkeletonInTopDomains("google.sk");
    EXPECT_EQ("google.sk", entry.domain);
    EXPECT_FALSE(entry.is_top_500);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kFull);
  }
}

// Check the unsafe version of IDNToUnicode. Even though the input domain
// matches a top domain, it should still be converted to unicode.
TEST(IDNSpoofCheckerNoFixtureTest, UnsafeIDNToUnicodeWithDetails) {
  const struct TestCase {
    // The IDNA/Punycode version of the domain (plain ASCII).
    const char* const punycode;
    // The equivalent Unicode version of the domain, if converted.
    const wchar_t* const expected_unicode;
    // Whether the input (punycode) has idn.
    const bool expected_has_idn;
    // The top domain that |punycode| matched to, if any.
    const char* const expected_matching_domain;
    // If true, the matching top domain is expected to be in top 500.
    const bool expected_is_top_500;
    const IDNSpoofChecker::Result expected_spoof_check_result;
  } kTestCases[] = {
      {// An ASCII, top domain.
       "google.com", L"google.com", false,
       // Since it's not unicode, we won't attempt to match it to a top domain.
       "",
       // ...And since we don't match it to a top domain, we don't know if it's
       // a top 500 domain.
       false, IDNSpoofChecker::Result::kNone},
      {// An ASCII domain that's not a top domain.
       "not-top-domain.com", L"not-top-domain.com", false, "", false,
       IDNSpoofChecker::Result::kNone},
      {// A unicode domain that's valid according to all of the rules in IDN
       // spoof checker except that it matches a top domain. Should be
       // converted to punycode. Spoof check result is kSafe because top domain
       // similarity isn't included in IDNSpoofChecker::Result.
       "xn--googl-fsa.com", L"googlé.com", true, "google.com", true,
       IDNSpoofChecker::Result::kSafe},
      {// A unicode domain that's not valid according to the rules in IDN spoof
       // checker (whole script confusable in Cyrillic) and it matches a top
       // domain. Should be converted to punycode.
       "xn--80ak6aa92e.com", L"аррӏе.com", true, "apple.com", true,
       IDNSpoofChecker::Result::kWholeScriptConfusable},
      {// A unicode domain that's not valid according to the rules in IDN spoof
       // checker (mixed script) but it doesn't match a top domain.
       "xn--o-o-oai-26a223aia177a7ab7649d.com", L"ɴoτ-τoρ-ďoᛖaiɴ.com", true, "",
       false, IDNSpoofChecker::Result::kICUSpoofChecks}};

  for (const TestCase& test_case : kTestCases) {
    const url_formatter::IDNConversionResult result =
        UnsafeIDNToUnicodeWithDetails(test_case.punycode);
    EXPECT_EQ(base::WideToUTF16(test_case.expected_unicode), result.result);
    EXPECT_EQ(test_case.expected_has_idn, result.has_idn_component);
    EXPECT_EQ(test_case.expected_matching_domain,
              result.matching_top_domain.domain);
    EXPECT_EQ(test_case.expected_is_top_500,
              result.matching_top_domain.is_top_500);
    EXPECT_EQ(test_case.expected_spoof_check_result, result.spoof_check_result);
  }
}

// Checks that skeletons are properly generated for domains with blocked
// characters after using UnsafeIDNToUnicodeWithDetails.
TEST(IDNSpoofCheckerNoFixtureTest, Skeletons) {
  // All of these should produce the same skeleton. Not all of these are
  // explicitly mapped in idn_spoof_checker.cc, ICU already handles some.
  const char kDashSite[] = "test-site";
  const struct TestCase {
    const GURL url;
    const char* const expected_skeleton;
  } kTestCases[] = {
      {GURL("http://test‐site"), kDashSite},   // U+2010 (Hyphen)
      {GURL("http://test‑site"), kDashSite},   // U+2011 (Non breaking hyphen)
      {GURL("http://test‒site"), kDashSite},   // U+2012 (Figure dash)
      {GURL("http://test–site"), kDashSite},   // U+2013 (En dash)
      {GURL("http://test—site"), kDashSite},   // U+2014 (Em dash)
      {GURL("http://test﹘site"), kDashSite},  // U+FE58 (Small em dash)
      {GURL("http://test―site"), kDashSite},   // U+2015 (Horizontal bar)
      {GURL("http://test一site"), kDashSite},  // U+4E00 (一)
      {GURL("http://test−site"), kDashSite},   // U+2212 (minus sign)
      {GURL("http://test⸺site"), kDashSite},   // U+2E3A (two-em dash)
      {GURL("http://test⸻site"), kDashSite},   // U+2E3B (three-em dash)
      {GURL("http://七est.net"), "test.net"},
      {GURL("http://丅est.net"), "test.net"},
      {GURL("http://丆est.net"), "test.net"},
      {GURL("http://c丫xe.com"), "cyxe.corn"},
      {GURL("http://cy乂e.com"), "cyxe.corn"},
      {GURL("http://丩3.com"), "43.corn"}};

  IDNSpoofChecker checker;
  for (const TestCase& test_case : kTestCases) {
    const url_formatter::IDNConversionResult result =
        UnsafeIDNToUnicodeWithDetails(test_case.url.host());
    Skeletons skeletons = checker.GetSkeletons(result.result);
    EXPECT_EQ(1u, skeletons.size());
    EXPECT_EQ(test_case.expected_skeleton, *skeletons.begin());
  }
}

TEST(IDNSpoofCheckerNoFixtureTest, MultipleSkeletons) {
  IDNSpoofChecker checker;
  // apple with U+04CF (ӏ)
  const GURL url("http://appӏe.com");
  const url_formatter::IDNConversionResult result =
      UnsafeIDNToUnicodeWithDetails(url.host());
  Skeletons skeletons = checker.GetSkeletons(result.result);
  EXPECT_EQ(Skeletons({"apple.corn", "appie.corn"}), skeletons);
}

}  // namespace url_formatter

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/idn_spoof_checker.h"

#include <stddef.h>
#include <string.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/url_formatter/spoof_checks/skeleton_generator.h"
#include "components/url_formatter/url_formatter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/uvernum.h"
#include "url/gurl.h"
#include "url/url_features.h"

namespace url_formatter {

namespace {

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
  const char16_t* unicode_output;
  // Whether we expect the domain to be displayed decoded as a Unicode string or
  // in its Punycode form.
  const Result expected_result;
};

// These cases MUST be generated with the script
// tools/security/idn_test_case_generator.py.
// See documentation there: you can either run it from the command line or call
// the make_case function directly from the Python shell (which may be easier
// for entering Unicode text).
//
// Do NOT generate these cases by hand.
//
// Q: Why not just do this conversion right here in the test, rather than having
//    a Python script to generate it?
// A: Because then we would have to rely on complex logic (IDNA encoding) in the
//    test itself; the same code we are trying to test. By using Python's IDN
//    encoder to generate the test data, we independently verify that our
//    algorithm is correct.
const IDNTestCase kIdnCases[] = {
    // No IDN
    {"www.google.com", u"www.google.com", kSafe},
    {"www.google.com.", u"www.google.com.", kSafe},
    {".", u".", kSafe},
    {"", u"", kSafe},
    // Invalid IDN
    {"xn--example-.com", u"xn--example-.com", kInvalid},
    // IDN
    // Hanzi (Traditional Chinese)
    {"xn--1lq90ic7f1rc.cn", u"\u5317\u4eac\u5927\u5b78.cn", kSafe},
    // Hanzi ('video' in Simplified Chinese)
    {"xn--cy2a840a.com", u"\u89c6\u9891.com", kSafe},
    // Hanzi + '123'
    {"www.xn--123-p18d.com", u"www.\u4e00123.com", kSafe},
    // Hanzi + Latin : U+56FD is simplified
    {"www.xn--hello-9n1hm04c.com", u"www.hello\u4e2d\u56fd.com", kSafe},
    // Kanji + Kana (Japanese)
    {"xn--l8jvb1ey91xtjb.jp", u"\u671d\u65e5\u3042\u3055\u3072.jp", kSafe},
    // Katakana including U+30FC
    {"xn--tckm4i2e.jp", u"\u30b3\u30de\u30fc\u30b9.jp", kSafe},
    {"xn--3ck7a7g.jp", u"\u30ce\u30f3\u30bd.jp", kSafe},
    // Katakana + Latin (Japanese)
    {"xn--e-efusa1mzf.jp", u"e\u30b3\u30de\u30fc\u30b9.jp", kSafe},
    {"xn--3bkxe.jp", u"\u30c8\u309a.jp", kSafe},
    // Hangul (Korean)
    {"www.xn--or3b17p6jjc.kr", u"www.\uc804\uc790\uc815\ubd80.kr", kSafe},
    // b<u-umlaut>cher (German)
    {"xn--bcher-kva.de", u"b\u00fccher.de", kSafe},
    // a with diaeresis
    {"www.xn--frgbolaget-q5a.se", u"www.f\u00e4rgbolaget.se", kSafe},
    // c-cedilla (French)
    {"www.xn--alliancefranaise-npb.fr", u"www.alliancefran\u00e7aise.fr",
     kSafe},
    // caf'e with acute accent (French)
    {"xn--caf-dma.fr", u"caf\u00e9.fr", kSafe},
    // c-cedillla and a with tilde (Portuguese)
    {"xn--poema-9qae5a.com.br", u"p\u00e3oema\u00e7\u00e3.com.br", kSafe},
    // s with caron
    {"xn--achy-f6a.com", u"\u0161achy.com", kSafe},
    {"xn--kxae4bafwg.gr", u"\u03bf\u03c5\u03c4\u03bf\u03c0\u03af\u03b1.gr",
     kSafe},
    // Eutopia + 123 (Greek)
    {"xn---123-pldm0haj2bk.gr",
     u"\u03bf\u03c5\u03c4\u03bf\u03c0\u03af\u03b1-123.gr", kSafe},
    // Cyrillic (Russian)
    {"xn--n1aeec9b.ru", u"\u0442\u043e\u0440\u0442\u044b.ru", kSafe},
    // Cyrillic + 123 (Russian)
    {"xn---123-45dmmc5f.ru", u"\u0442\u043e\u0440\u0442\u044b-123.ru", kSafe},
    // 'president' in Russian. Is a wholescript confusable, but allowed.
    {"xn--d1abbgf6aiiy.xn--p1ai",
     u"\u043f\u0440\u0435\u0437\u0438\u0434\u0435\u043d\u0442.\u0440\u0444",
     kSafe},
    // Arabic
    {"xn--mgba1fmg.eg", u"\u0627\u0641\u0644\u0627\u0645.eg", kSafe},
    // Hebrew
    {"xn--4dbib.he", u"\u05d5\u05d0\u05d4.he", kSafe},
    // Hebrew + Common
    {"xn---123-ptf2c5c6bt.il", u"\u05e2\u05d1\u05e8\u05d9\u05ea-123.il", kSafe},
    // Thai
    {"xn--12c2cc4ag3b4ccu.th",
     u"\u0e2a\u0e32\u0e22\u0e01\u0e32\u0e23\u0e1a\u0e34\u0e19.th", kSafe},
    // Thai + Common
    {"xn---123-9goxcp8c9db2r.th",
     u"\u0e20\u0e32\u0e29\u0e32\u0e44\u0e17\u0e22-123.th", kSafe},
    // Devangari (Hindi)
    {"www.xn--l1b6a9e1b7c.in", u"www.\u0905\u0915\u094b\u0932\u093e.in", kSafe},
    // Devanagari + Common
    {"xn---123-kbjl2j0bl2k.in", u"\u0939\u093f\u0928\u094d\u0926\u0940-123.in",
     kSafe},

    // Block mixed numeric + numeric lookalike (12.com, using U+0577).
    {"xn--1-xcc.com", u"1\u0577.com", kUnsafe},

    // Block mixed numeric lookalike + numeric (੨0.com, uses U+0A68).
    {"xn--0-6ee.com", u"\u0a680.com", kUnsafe},
    // Block fully numeric lookalikes (৪੨.com using U+09EA and U+0A68).
    {"xn--47b6w.com", u"\u09ea\u0a68.com", kUnsafe},
    // Block single script digit lookalikes (using three U+0A68 characters).
    {"xn--qccaa.com", u"\u0a68\u0a68\u0a68.com", kUnsafe},

    // URL test with mostly numbers and one confusable character
    // Georgian 'd' 4000.com
    {"xn--4000-pfr.com", u"\u10eb4000.com", kUnsafe},

    // What used to be 5 Aspirational scripts in the earlier versions of UAX 31.
    // UAX 31 does not define aspirational scripts any more.
    // See http://www.unicode.org/reports/tr31/#Aspirational_Use_Scripts .
    // Unified Canadian Syllabary
    {"xn--dfe0tte.ca", u"\u1456\u14c2\u14ef.ca", kUnsafe},
    // Tifinagh
    {"xn--4ljxa2bb4a6bxb.ma", u"\u2d5c\u2d49\u2d3c\u2d49\u2d4f\u2d30\u2d56.ma",
     kUnsafe},
    // Tifinagh with a disallowed character(U+2D6F)
    {"xn--hmjzaby5d5f.ma", u"\u2d5c\u2d49\u2d3c\u2d6f\u2d49\u2d4f.ma",
     kInvalid},

    // Yi
    {"xn--4o7a6e1x64c.cn", u"\ua188\ua320\ua071\ua0b7.cn", kUnsafe},
    // Mongolian - 'ordu' (place, camp)
    {"xn--56ec8bp.cn", u"\u1823\u1837\u1833\u1824.cn", kUnsafe},
    // Mongolian with a disallowed character
    {"xn--95e5de3ds.cn", u"\u1823\u1837\u1804\u1833\u1824.cn", kUnsafe},
    // Miao/Pollad
    {"xn--2u0fpf0a.cn", u"\U00016f04\U00016f62\U00016f59.cn", kUnsafe},

    // Script mixing tests
    // The following script combinations are allowed.
    // HIGHLY_RESTRICTIVE with Latin limited to ASCII-Latin.
    // ASCII-Latin + Japn (Kana + Han)
    // ASCII-Latin + Kore (Hangul + Han)
    // ASCII-Latin + Han + Bopomofo
    // "payp<alpha>l.com"
    {"xn--paypl-g9d.com", u"payp\u03b1l.com", kUnsafe},
    // google.gr with Greek omicron and epsilon
    {"xn--ggl-6xc1ca.gr", u"g\u03bf\u03bfgl\u03b5.gr", kUnsafe},
    // google.ru with Cyrillic o
    {"xn--ggl-tdd6ba.ru", u"g\u043e\u043egl\u0435.ru", kUnsafe},
    // h<e with acute>llo<China in Han>.cn
    {"xn--hllo-bpa7979ih5m.cn", u"h\u00e9llo\u4e2d\u56fd.cn", kUnsafe},
    // <Greek rho><Cyrillic a><Cyrillic u>.ru
    {"xn--2xa6t2b.ru", u"\u03c1\u0430\u0443.ru", kUnsafe},
    // Georgian + Latin
    {"xn--abcef-vuu.test", u"abc\u10ebef.test", kUnsafe},
    // Hangul + Latin
    {"xn--han-eb9ll88m.kr", u"\ud55c\uae00han.kr", kSafe},
    // Hangul + Latin + Han with IDN ccTLD
    {"xn--han-or0kq92gkm3c.xn--3e0b707e", u"\ud55c\uae00han\u97d3.\ud55c\uad6d",
     kSafe},
    // non-ASCII Latin + Hangul
    {"xn--caf-dma9024xvpg.kr", u"caf\u00e9\uce74\ud398.kr", kUnsafe},
    // Hangul + Hiragana
    {"xn--y9j3b9855e.kr", u"\ud55c\u3072\u3089.kr", kUnsafe},
    // <Hiragana>.<Hangul> is allowed because script mixing check is per label.
    {"xn--y9j3b.xn--3e0b707e", u"\u3072\u3089.\ud55c\uad6d", kSafe},
    //  Traditional Han + Latin
    {"xn--hanzi-u57ii69i.tw", u"\u6f22\u5b57hanzi.tw", kSafe},
    //  Simplified Han + Latin
    {"xn--hanzi-u57i952h.cn", u"\u6c49\u5b57hanzi.cn", kSafe},
    // Simplified Han + Traditonal Han
    {"xn--hanzi-if9kt8n.cn", u"\u6c49\u6f22hanzi.cn", kSafe},
    //  Han + Hiragana + Katakana + Latin
    {"xn--kanji-ii4dpizfq59yuykqr4b.jp",
     u"\u632f\u308a\u4eee\u540d\u30ab\u30bfkanji.jp", kSafe},
    // Han + Bopomofo
    {"xn--5ekcde0577e87tc.tw", u"\u6ce8\u97f3\u3105\u3106\u3107\u3108.tw",
     kSafe},
    // Han + Latin + Bopomofo
    {"xn--bopo-ty4cghi8509kk7xd.tw",
     u"\u6ce8\u97f3bopo\u3105\u3106\u3107\u3108.tw", kSafe},
    // Latin + Bopomofo
    {"xn--bopomofo-hj5gkalm.tw", u"bopomofo\u3105\u3106\u3107\u3108.tw", kSafe},
    // Bopomofo + Katakana
    {"xn--lcka3d1bztghi.tw",
     u"\u3105\u3106\u3107\u3108\u30ab\u30bf\u30ab\u30ca.tw", kUnsafe},
    //  Bopomofo + Hangul
    {"xn--5ekcde4543qbec.tw", u"\u3105\u3106\u3107\u3108\uc8fc\uc74c.tw",
     kUnsafe},
    // Devanagari + Latin
    {"xn--ab-3ofh8fqbj6h.in", u"ab\u0939\u093f\u0928\u094d\u0926\u0940.in",
     kUnsafe},
    // Thai + Latin
    {"xn--ab-jsi9al4bxdb6n.th",
     u"ab\u0e20\u0e32\u0e29\u0e32\u0e44\u0e17\u0e22.th", kUnsafe},
    // Armenian + Latin
    {"xn--bs-red.com", u"b\u057ds.com", kUnsafe},
    // Tibetan + Latin
    {"xn--foo-vkm.com", u"foo\u0f37.com", kUnsafe},
    // Oriya + Latin
    {"xn--fo-h3g.com", u"fo\u0b66.com", kUnsafe},
    // Gujarati + Latin
    {"xn--fo-isg.com", u"fo\u0ae6.com", kUnsafe},
    // <vitamin in Katakana>b1.com
    {"xn--b1-xi4a7cvc9f.com", u"\u30d3\u30bf\u30df\u30f3b1.com", kSafe},
    // Devanagari + Han
    {"xn--t2bes3ds6749n.com", u"\u0930\u094b\u0932\u0947\u76e7\u0938.com",
     kUnsafe},
    // Devanagari + Bengali
    {"xn--11b0x.in", u"\u0915\u0995.in", kUnsafe},
    // Canadian Syllabary + Latin
    {"xn--ab-lym.com", u"ab\u14bf.com", kUnsafe},
    {"xn--ab1-p6q.com", u"ab1\u14bf.com", kUnsafe},
    {"xn--1ab-m6qd.com", u"\u14bf1ab\u14bf.com", kUnsafe},
    {"xn--ab-jymc.com", u"\u14bfab\u14bf.com", kUnsafe},
    // Tifinagh + Latin
    {"xn--liy-bq1b.com", u"li\u2d4fy.com", kUnsafe},
    {"xn--rol-cq1b.com", u"rol\u2d4f.com", kUnsafe},
    {"xn--ily-8p1b.com", u"\u2d4fily.com", kUnsafe},
    {"xn--1ly-8p1b.com", u"\u2d4f1ly.com", kUnsafe},

    // Invisibility check
    // Thai tone mark malek(U+0E48) repeated
    {"xn--03c0b3ca.th", u"\u0e23\u0e35\u0e48\u0e48.th", kUnsafe},
    // Accute accent repeated
    {"xn--a-xbba.com", u"a\u0301\u0301.com", kInvalid},
    // 'a' with acuted accent + another acute accent
    {"xn--1ca20i.com", u"\u00e1\u0301.com", kUnsafe},
    // Combining mark at the beginning
    {"xn--abc-fdc.jp", u"\u0300abc.jp", kInvalid},

    // The following three are detected by |dangerous_pattern| regex, but
    // can be regarded as an extension of blocking repeated diacritic marks.
    // i followed by U+0307 (combining dot above)
    {"xn--pixel-8fd.com", u"pi\u0307xel.com", kUnsafe},
    // U+0131 (dotless i) followed by U+0307
    {"xn--pxel-lza43z.com", u"p\u0131\u0307xel.com", kUnsafe},
    // j followed by U+0307 (combining dot above)
    {"xn--jack-qwc.com", u"j\u0307ack.com", kUnsafe},
    // l followed by U+0307
    {"xn--lace-qwc.com", u"l\u0307ace.com", kUnsafe},

    // Do not allow a combining mark after dotless i/j.
    {"xn--pxel-lza29y.com", u"p\u0131\u0300xel.com", kUnsafe},
    {"xn--ack-gpb42h.com", u"\u0237\u0301ack.com", kUnsafe},

    // Mixed script confusable
    // google with Armenian Small Letter Oh(U+0585)
    {"xn--gogle-lkg.com", u"g\u0585ogle.com", kUnsafe},
    {"xn--range-kkg.com", u"\u0585range.com", kUnsafe},
    {"xn--cucko-pkg.com", u"cucko\u0585.com", kUnsafe},
    // Latin 'o' in Armenian.
    {"xn--o-ybcg0cu0cq.com", u"o\u0580\u0574\u0578\u0582\u0566\u0568.com",
     kUnsafe},
    // Hiragana HE(U+3078) mixed with Katakana
    {"xn--49jxi3as0d0fpc.com",
     u"\u30e2\u30d2\u30fc\u30c8\u3078\u30d6\u30f3.com", kUnsafe},

    // U+30FC should be preceded by a Hiragana/Katakana.
    // Katakana + U+30FC + Han
    {"xn--lck0ip02qw5ya.jp", u"\u30ab\u30fc\u91ce\u7403.jp", kSafe},
    // Hiragana + U+30FC + Han
    {"xn--u8j5tr47nw5ya.jp", u"\u304b\u30fc\u91ce\u7403.jp", kSafe},
    // U+30FC + Han
    {"xn--weka801xo02a.com", u"\u30fc\u52d5\u753b\u30fc.com", kUnsafe},
    // Han + U+30FC + Han
    {"xn--wekz60nb2ay85atj0b.jp", u"\u65e5\u672c\u30fc\u91ce\u7403.jp",
     kUnsafe},
    // U+30FC at the beginning
    {"xn--wek060nb2a.jp", u"\u30fc\u65e5\u672c.jp", kUnsafe},
    // Latin + U+30FC + Latin
    {"xn--abcdef-r64e.jp", u"abc\u30fcdef.jp", kUnsafe},

    // U+30FB (・) is not allowed next to Latin, but allowed otherwise.
    // U+30FB + Han
    {"xn--vekt920a.jp", u"\u30fb\u91ce.jp", kSafe},
    // Han + U+30FB + Han
    {"xn--vek160nb2ay85atj0b.jp", u"\u65e5\u672c\u30fb\u91ce\u7403.jp", kSafe},
    // Latin + U+30FB + Latin
    {"xn--abcdef-k64e.jp", u"abc\u30fbdef.jp", kUnsafe},
    // U+30FB + Latin
    {"xn--abc-os4b.jp", u"\u30fbabc.jp", kUnsafe},

    // U+30FD (ヽ) is allowed only after Katakana.
    // Katakana + U+30FD
    {"xn--lck2i.jp", u"\u30ab\u30fd.jp", kSafe},
    // Hiragana + U+30FD
    {"xn--u8j7t.jp", u"\u304b\u30fd.jp", kUnsafe},
    // Han + U+30FD
    {"xn--xek368f.jp", u"\u4e00\u30fd.jp", kUnsafe},
    {"xn--a-mju.jp", u"a\u30fd.jp", kUnsafe},
    {"xn--a1-bo4a.jp", u"a1\u30fd.jp", kUnsafe},

    // U+30FE (ヾ) is allowed only after Katakana.
    // Katakana + U+30FE
    {"xn--lck4i.jp", u"\u30ab\u30fe.jp", kSafe},
    // Hiragana + U+30FE
    {"xn--u8j9t.jp", u"\u304b\u30fe.jp", kUnsafe},
    // Han + U+30FE
    {"xn--yek168f.jp", u"\u4e00\u30fe.jp", kUnsafe},
    {"xn--a-oju.jp", u"a\u30fe.jp", kUnsafe},
    {"xn--a1-eo4a.jp", u"a1\u30fe.jp", kUnsafe},

    // Cyrillic labels made of Latin-look-alike Cyrillic letters.
    // 1) ѕсоре.com with ѕсоре in Cyrillic.
    {"xn--e1argc3h.com", u"\u0455\u0441\u043e\u0440\u0435.com", kUnsafe},
    // 2) ѕсоре123.com with ѕсоре in Cyrillic.
    {"xn--123-qdd8bmf3n.com", u"\u0455\u0441\u043e\u0440\u0435123.com",
     kUnsafe},
    // 3) ѕсоре-рау.com with ѕсоре and рау in Cyrillic.
    {"xn----8sbn9akccw8m.com",
     u"\u0455\u0441\u043e\u0440\u0435-\u0440\u0430\u0443.com", kUnsafe},
    // 4) ѕсоре1рау.com with scope and pay in Cyrillic and a non-letter between
    // them.
    {"xn--1-8sbn9akccw8m.com",
     u"\u0455\u0441\u043e\u0440\u0435\u0031\u0440\u0430\u0443.com", kUnsafe},
    // курс.com is a whole-script-confusable but курс is an allowed word.
    {"xn--j1amdg.com", u"\u043a\u0443\u0440\u0441.com", kSafe},
    // ск.com is a whole-script-confusable.
    {"xn--j1an.com", u"\u0441\u043a.com", kUnsafe},

    // The same as above three, but in IDN TLD (рф).
    // 1) ѕсоре.рф with ѕсоре in Cyrillic.
    {"xn--e1argc3h.xn--p1ai", u"\u0455\u0441\u043e\u0440\u0435.\u0440\u0444",
     kSafe},
    // 2) ѕсоре123.рф with ѕсоре in Cyrillic.
    {"xn--123-qdd8bmf3n.xn--p1ai",
     u"\u0455\u0441\u043e\u0440\u0435123.\u0440\u0444", kSafe},
    // 3) ѕсоре-рау.рф with ѕсоре and рау in Cyrillic.
    {"xn----8sbn9akccw8m.xn--p1ai",
     u"\u0455\u0441\u043e\u0440\u0435-\u0440\u0430\u0443.\u0440\u0444", kSafe},
    // 4) ѕсоре1рау.com with scope and pay in Cyrillic and a non-letter between
    // them.
    {"xn--1-8sbn9akccw8m.xn--p1ai",
     u"\u0455\u0441\u043e\u0440\u0435\u0031\u0440\u0430\u0443.\u0440\u0444",
     kSafe},

    // Same as above three, but in .ru TLD.
    // 1) ѕсоре.ru  with ѕсоре in Cyrillic.
    {"xn--e1argc3h.ru", u"\u0455\u0441\u043e\u0440\u0435.ru", kSafe},
    // 2) ѕсоре123.ru with ѕсоре in Cyrillic.
    {"xn--123-qdd8bmf3n.ru", u"\u0455\u0441\u043e\u0440\u0435123.ru", kSafe},
    // 3) ѕсоре-рау.ru with ѕсоре and рау in Cyrillic.
    {"xn----8sbn9akccw8m.ru",
     u"\u0455\u0441\u043e\u0440\u0435-\u0440\u0430\u0443.ru", kSafe},
    // 4) ѕсоре1рау.com with scope and pay in Cyrillic and a non-letter between
    // them.
    {"xn--1-8sbn9akccw8m.ru",
     u"\u0455\u0441\u043e\u0440\u0435\u0031\u0440\u0430\u0443.ru", kSafe},

    // ѕсоре-рау.한국 with ѕсоре and рау in Cyrillic. The label will remain
    // punycode while the TLD will be decoded.
    {"xn----8sbn9akccw8m.xn--3e0b707e", u"xn----8sbn9akccw8m.\ud55c\uad6d",
     kSafe},

    // музей (museum in Russian) has characters without a Latin-look-alike.
    {"xn--e1adhj9a.com", u"\u043c\u0443\u0437\u0435\u0439.com", kSafe},

    // ѕсоԗе.com is Cyrillic with Latin lookalikes.
    {"xn--e1ari3f61c.com", u"\u0455\u0441\u043e\u0517\u0435.com", kUnsafe},

    // ыоԍ.com is Cyrillic with Latin lookalikes.
    {"xn--n1az74c.com", u"\u044b\u043e\u050d.com", kUnsafe},

    // сю.com is Cyrillic with Latin lookalikes.
    {"xn--q1a0a.com", u"\u0441\u044e.com", kUnsafe},

    // Regression test for lowercase letters in whole script confusable
    // lookalike character lists.
    {"xn--80a8a6a.com", u"\u0430\u044c\u0441.com", kUnsafe},

    // googlе.한국 where е is Cyrillic. This tests the generic case when one
    // label is not allowed but  other labels in the domain name are still
    // decoded. Here, googlе is left in punycode but the TLD is decoded.
    {"xn--googl-3we.xn--3e0b707e", u"xn--googl-3we.\ud55c\uad6d", kSafe},

    // Combining Diacritic marks after a script other than Latin-Greek-Cyrillic
    {"xn--rsa2568fvxya.com", u"\ud55c\u0307\uae00.com", kUnsafe},  // 한́글.com
    {"xn--rsa0336bjom.com", u"\u6f22\u0307\u5b57.com", kUnsafe},  // 漢̇字.com
    // नागरी́.com
    {"xn--lsa922apb7a6do.com", u"\u0928\u093e\u0917\u0930\u0940\u0301.com",
     kUnsafe},

    // Similarity checks against the list of top domains. "digklmo68.com" and
    // 'digklmo68.co.uk" are listed for unittest in the top domain list.
    // đigklmo68.com:
    {"xn--igklmo68-kcb.com", u"\u0111igklmo68.com", kUnsafe},
    // www.đigklmo68.com:
    {"www.xn--igklmo68-kcb.com", u"www.\u0111igklmo68.com", kUnsafe},
    // foo.bar.đigklmo68.com:
    {"foo.bar.xn--igklmo68-kcb.com", u"foo.bar.\u0111igklmo68.com", kUnsafe},
    // đigklmo68.co.uk:
    {"xn--igklmo68-kcb.co.uk", u"\u0111igklmo68.co.uk", kUnsafe},
    // mail.đigklmo68.co.uk:
    {"mail.xn--igklmo68-kcb.co.uk", u"mail.\u0111igklmo68.co.uk", kUnsafe},
    // di̇gklmo68.com:
    {"xn--digklmo68-6jf.com", u"di\u0307gklmo68.com", kUnsafe},
    // dig̱klmo68.com:
    {"xn--digklmo68-7vf.com", u"dig\u0331klmo68.com", kUnsafe},
    // digĸlmo68.com:
    {"xn--diglmo68-omb.com", u"dig\u0138lmo68.com", kUnsafe},
    // digkłmo68.com:
    {"xn--digkmo68-9ob.com", u"digk\u0142mo68.com", kUnsafe},
    // digklṃo68.com:
    {"xn--digklo68-l89c.com", u"digkl\u1e43o68.com", kUnsafe},
    // digklmø68.com:
    {"xn--digklm68-b5a.com", u"digklm\u00f868.com", kUnsafe},
    // digklmoб8.com:
    {"xn--digklmo8-h7g.com", u"digklmo\u04318.com", kUnsafe},
    // digklmo6৪.com:
    {"xn--digklmo6-7yr.com", u"digklmo6\u09ea.com", kUnsafe},

    // 'islkpx123.com' is in the test domain list.
    // 'іѕӏкрх123' can look like 'islkpx123' in some fonts.
    {"xn--123-bed4a4a6hh40i.com",
     u"\u0456\u0455\u04cf\u043a\u0440\u0445123.com", kUnsafe},

    // 'o2.com', '28.com', '39.com', '43.com', '89.com', 'oo.com' and 'qq.com'
    // are all explicitly added to the test domain list to aid testing of
    // Latin-lookalikes that are numerics in other character sets and similar
    // edge cases.
    //
    // Bengali:
    {"xn--07be.com", u"\u09e6\u09e8.com", kUnsafe},
    {"xn--27be.com", u"\u09e8\u09ea.com", kUnsafe},
    {"xn--77ba.com", u"\u09ed\u09ed.com", kUnsafe},
    // Gurmukhi:
    {"xn--qcce.com", u"\u0a68\u0a6a.com", kUnsafe},
    {"xn--occe.com", u"\u0a66\u0a68.com", kUnsafe},
    {"xn--rccd.com", u"\u0a6b\u0a69.com", kUnsafe},
    {"xn--pcca.com", u"\u0a67\u0a67.com", kUnsafe},
    // Telugu:
    {"xn--drcb.com", u"\u0c69\u0c68.com", kUnsafe},
    // Devanagari:
    {"xn--d4be.com", u"\u0966\u0968.com", kUnsafe},
    // Kannada:
    {"xn--yucg.com", u"\u0ce6\u0ce9.com", kUnsafe},
    {"xn--yuco.com", u"\u0ce6\u0ced.com", kUnsafe},
    // Oriya:
    {"xn--1jcf.com", u"\u0b6b\u0b68.com", kUnsafe},
    {"xn--zjca.com", u"\u0b66\u0b66.com", kUnsafe},
    // Gujarati:
    {"xn--cgce.com", u"\u0ae6\u0ae8.com", kUnsafe},
    {"xn--fgci.com", u"\u0ae9\u0aed.com", kUnsafe},
    {"xn--dgca.com", u"\u0ae7\u0ae7.com", kUnsafe},

    // wmhtb.com
    {"xn--l1acpvx.com", u"\u0448\u043c\u043d\u0442\u044c.com", kUnsafe},
    // щмнть.com
    {"xn--l1acpzs.com", u"\u0449\u043c\u043d\u0442\u044c.com", kUnsafe},
    // шмнтв.com
    {"xn--b1atdu1a.com", u"\u0448\u043c\u043d\u0442\u0432.com", kUnsafe},
    // шмԋтв.com
    {"xn--b1atsw09g.com", u"\u0448\u043c\u050b\u0442\u0432.com", kUnsafe},
    // шмԧтв.com
    {"xn--b1atsw03i.com", u"\u0448\u043c\u0527\u0442\u0432.com", kUnsafe},
    // шмԋԏв.com
    {"xn--b1at9a12dua.com", u"\u0448\u043c\u050b\u050f\u0432.com", kUnsafe},
    // ഠട345.com
    {"xn--345-jtke.com", u"\u0d20\u0d1f345.com", kUnsafe},

    // Test additional confusable LGC characters (most of them without
    // decomposition into base + diacritc mark). The corresponding ASCII
    // domain names are in the test top domain list.
    // ϼκαωχ.com
    {"xn--mxar4bh6w.com", u"\u03fc\u03ba\u03b1\u03c9\u03c7.com", kUnsafe},
    // þħĸŧƅ.com
    {"xn--vda6f3b2kpf.com", u"\u00fe\u0127\u0138\u0167\u0185.com", kUnsafe},
    // þhktb.com
    {"xn--hktb-9ra.com", u"\u00fehktb.com", kUnsafe},
    // pħktb.com
    {"xn--pktb-5xa.com", u"p\u0127ktb.com", kUnsafe},
    // phĸtb.com
    {"xn--phtb-m0a.com", u"ph\u0138tb.com", kUnsafe},
    // phkŧb.com
    {"xn--phkb-d7a.com", u"phk\u0167b.com", kUnsafe},
    // phktƅ.com
    {"xn--phkt-ocb.com", u"phkt\u0185.com", kUnsafe},
    // ҏнкть.com
    {"xn--j1afq4bxw.com", u"\u048f\u043d\u043a\u0442\u044c.com", kUnsafe},
    // ҏћкть.com
    {"xn--j1aq4a7cvo.com", u"\u048f\u045b\u043a\u0442\u044c.com", kUnsafe},
    // ҏңкть.com
    {"xn--j1aq4azund.com", u"\u048f\u04a3\u043a\u0442\u044c.com", kUnsafe},
    // ҏҥкть.com
    {"xn--j1aq4azuxd.com", u"\u048f\u04a5\u043a\u0442\u044c.com", kUnsafe},
    // ҏӈкть.com
    {"xn--j1aq4azuyj.com", u"\u048f\u04c8\u043a\u0442\u044c.com", kUnsafe},
    // ҏԧкть.com
    {"xn--j1aq4azu9z.com", u"\u048f\u0527\u043a\u0442\u044c.com", kUnsafe},
    // ҏԩкть.com
    {"xn--j1aq4azuq0a.com", u"\u048f\u0529\u043a\u0442\u044c.com", kUnsafe},
    // ҏнқть.com
    {"xn--m1ak4azu6b.com", u"\u048f\u043d\u049b\u0442\u044c.com", kUnsafe},
    // ҏнҝть.com
    {"xn--m1ak4azunc.com", u"\u048f\u043d\u049d\u0442\u044c.com", kUnsafe},
    // ҏнҟть.com
    {"xn--m1ak4azuxc.com", u"\u048f\u043d\u049f\u0442\u044c.com", kUnsafe},
    // ҏнҡть.com
    {"xn--m1ak4azu7c.com", u"\u048f\u043d\u04a1\u0442\u044c.com", kUnsafe},
    // ҏнӄть.com
    {"xn--m1ak4azu8i.com", u"\u048f\u043d\u04c4\u0442\u044c.com", kUnsafe},
    // ҏнԟть.com
    {"xn--m1ak4azuzy.com", u"\u048f\u043d\u051f\u0442\u044c.com", kUnsafe},
    // ҏнԟҭь.com
    {"xn--m1a4a4nnery.com", u"\u048f\u043d\u051f\u04ad\u044c.com", kUnsafe},
    // ҏнԟҭҍ.com
    {"xn--m1a4ne5jry.com", u"\u048f\u043d\u051f\u04ad\u048d.com", kUnsafe},
    // ҏнԟҭв.com
    {"xn--b1av9v8dry.com", u"\u048f\u043d\u051f\u04ad\u0432.com", kUnsafe},
    // ҏӊԟҭв.com
    {"xn--b1a9p8c1e8r.com", u"\u048f\u04ca\u051f\u04ad\u0432.com", kUnsafe},
    // wmŋr.com
    {"xn--wmr-jxa.com", u"wm\u014br.com", kUnsafe},
    // шмпґ.com
    {"xn--l1agz80a.com", u"\u0448\u043c\u043f\u0491.com", kUnsafe},
    // щмпґ.com
    {"xn--l1ag2a0y.com", u"\u0449\u043c\u043f\u0491.com", kUnsafe},
    // щӎпґ.com
    {"xn--o1at1tsi.com", u"\u0449\u04ce\u043f\u0491.com", kUnsafe},
    // ґғ.com
    {"xn--03ae.com", u"\u0491\u0493.com", kUnsafe},
    // ґӻ.com
    {"xn--03a6s.com", u"\u0491\u04fb.com", kUnsafe},
    // ҫұҳҽ.com
    {"xn--r4amg4b.com", u"\u04ab\u04b1\u04b3\u04bd.com", kUnsafe},
    // ҫұӽҽ.com
    {"xn--r4am0b8r.com", u"\u04ab\u04b1\u04fd\u04bd.com", kUnsafe},
    // ҫұӿҽ.com
    {"xn--r4am0b3s.com", u"\u04ab\u04b1\u04ff\u04bd.com", kUnsafe},
    // ҫұӿҿ.com
    {"xn--r4am6b4p.com", u"\u04ab\u04b1\u04ff\u04bf.com", kUnsafe},
    // ҫұӿє.com
    {"xn--91a7osa62a.com", u"\u04ab\u04b1\u04ff\u0454.com", kUnsafe},
    // ӏԃԍ.com
    {"xn--s5a8h4a.com", u"\u04cf\u0503\u050d.com", kUnsafe},

    // U+04CF(ӏ) is mapped to multiple characters, lowercase L(l) and
    // lowercase I(i). Lowercase L is also regarded as similar to digit 1.
    // The test domain list has {ig, ld, 1gd}.com for Cyrillic.
    // ӏԍ.com
    {"xn--s5a8j.com", u"\u04cf\u050d.com", kUnsafe},
    // ӏԃ.com
    {"xn--s5a8h.com", u"\u04cf\u0503.com", kUnsafe},
    // ӏԍԃ.com
    {"xn--s5a8h3a.com", u"\u04cf\u050d\u0503.com", kUnsafe},

    // 1շ34567890.com
    {"xn--134567890-gnk.com", u"1\u057734567890.com", kUnsafe},
    // ꓲ2345б7890.com
    {"xn--23457890-e7g93622b.com", u"\ua4f22345\u04317890.com", kUnsafe},
    // 1ᒿ345б7890.com
    {"xn--13457890-e7g0943b.com", u"1\u14bf345\u04317890.com", kUnsafe},
    // 12з4567890.com
    {"xn--124567890-10h.com", u"12\u04374567890.com", kUnsafe},
    // 12ҙ4567890.com
    {"xn--124567890-1ti.com", u"12\u04994567890.com", kUnsafe},
    // 12ӡ4567890.com
    {"xn--124567890-mfj.com", u"12\u04e14567890.com", kUnsafe},
    // 12उ4567890.com
    {"xn--124567890-m3r.com", u"12\u09094567890.com", kUnsafe},
    // 12ও4567890.com
    {"xn--124567890-17s.com", u"12\u09934567890.com", kUnsafe},
    // 12ਤ4567890.com
    {"xn--124567890-hfu.com", u"12\u0a244567890.com", kUnsafe},
    // 12ဒ4567890.com
    {"xn--124567890-6s6a.com", u"12\u10124567890.com", kUnsafe},
    // 12ვ4567890.com
    {"xn--124567890-we8a.com", u"12\u10D54567890.com", kUnsafe},
    // 12პ4567890.com
    {"xn--124567890-hh8a.com", u"12\u10DE4567890.com", kUnsafe},
    // 123ㄐ567890.com
    {"xn--123567890-dr5h.com", u"123ㄐ567890.com", kUnsafe},
    // 123Ꮞ567890.com
    {"xn--123567890-dm4b.com", u"123\u13ce567890.com", kUnsafe},
    // 12345б7890.com
    {"xn--123457890-fzh.com", u"12345\u04317890.com", kUnsafe},
    // 12345ճ7890.com
    {"xn--123457890-fmk.com", u"12345ճ7890.com", kUnsafe},
    // 1234567ȣ90.com
    {"xn--123456790-6od.com", u"1234567\u022390.com", kUnsafe},
    // 12345678୨0.com
    {"xn--123456780-71w.com", u"12345678\u0b680.com", kUnsafe},
    // 123456789ଠ.com
    {"xn--123456789-ohw.com", u"123456789\u0b20.com", kUnsafe},
    // 123456789ꓳ.com
    {"xn--123456789-tx75a.com", u"123456789\ua4f3.com", kUnsafe},

    // aeœ.com
    {"xn--ae-fsa.com", u"ae\u0153.com", kUnsafe},
    // æce.com
    {"xn--ce-0ia.com", u"\u00e6ce.com", kUnsafe},
    // æœ.com
    {"xn--6ca2t.com", u"\u00e6\u0153.com", kUnsafe},
    // ӕԥ.com
    {"xn--y5a4n.com", u"\u04d5\u0525.com", kUnsafe},

    // ငၔဌ၂ဝ.com (entirely made of Myanmar characters)
    {"xn--ridq5c9hnd.com", u"\u1004\u1054\u100c\u1042\u101d.com", kUnsafe},

    // ฟรฟร.com (made of two Thai characters. similar to wsws.com in
    // some fonts)
    {"xn--w3calb.com", u"\u0e1f\u0e23\u0e1f\u0e23.com", kUnsafe},
    // พรบ.com
    {"xn--r3chp.com", u"\u0e1e\u0e23\u0e1a.com", kUnsafe},
    // ฟรบ.com
    {"xn--r3cjm.com", u"\u0e1f\u0e23\u0e1a.com", kUnsafe},

    // Lao characters that look like w, s, o, and u.
    // ພຣບ.com
    {"xn--f7chp.com", u"\u0e9e\u0ea3\u0e9a.com", kUnsafe},
    // ຟຣບ.com
    {"xn--f7cjm.com", u"\u0e9f\u0ea3\u0e9a.com", kUnsafe},
    // ຟຮບ.com
    {"xn--f7cj9b.com", u"\u0e9f\u0eae\u0e9a.com", kUnsafe},
    // ຟຮ໐ບ.com
    {"xn--f7cj9b5h.com", u"\u0e9f\u0eae\u0ed0\u0e9a.com", kUnsafe},

    // Lao character that looks like n.
    // ก11.com
    {"xn--11-lqi.com", u"\u0e0111.com", kUnsafe},

    // At one point the skeleton of 'w' was 'vv', ensure that
    // that it's treated as 'w'.
    {"xn--wder-qqa.com", u"w\u00f3der.com", kUnsafe},

    // Mixed digits: the first two will also fail mixed script test
    // Latin + ASCII digit + Deva digit
    {"xn--asc1deva-j0q.co.in", u"asc1deva\u0967.co.in", kUnsafe},
    // Latin + Deva digit + Beng digit
    {"xn--devabeng-f0qu3f.co.in", u"deva\u0967beng\u09e7.co.in", kUnsafe},
    // ASCII digit + Deva digit
    {"xn--79-v5f.co.in", u"7\u09ea9.co.in", kUnsafe},
    //  Deva digit + Beng digit
    {"xn--e4b0x.co.in", u"\u0967\u09e7.co.in", kUnsafe},
    // U+4E00 (CJK Ideograph One) is not a digit, but it's not allowed next to
    // non-Kana scripts including numbers.
    {"xn--d12-s18d.cn", u"d12\u4e00.cn", kUnsafe},
    // One that's really long that will force a buffer realloc
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaa",
     u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     u"aaaaaaaa",
     kSafe},

    // Not allowed; characters outside [:Identifier_Status=Allowed:]
    // Limited Use Scripts: UTS 31 Table 7.
    // Vai
    {"xn--sn8a.com", u"\ua50b.com", kUnsafe},
    // 'CARD' look-alike in Cherokee
    {"xn--58db0a9q.com", u"\u13df\u13aa\u13a1\u13a0.com", kUnsafe},
    // Scripts excluded from Identifiers: UTS 31 Table 4
    // Coptic
    {"xn--5ya.com", u"\u03e7.com", kUnsafe},
    // Old Italic
    {"xn--097cc.com", u"\U00010300\U00010301.com", kUnsafe},

    // U+115F (Hangul Filler)
    {"xn--osd3820f24c.kr", u"\uac00\ub098\u115f.kr", kInvalid},
    {"www.xn--google-ho0coa.com", u"www.\u2039google\u203a.com", kUnsafe},
    // Latin small capital w: hardᴡare.com
    {"xn--hardare-l41c.com", u"hard\u1d21are.com", kUnsafe},
    // Minus Sign(U+2212)
    {"xn--t9g238xc2a.jp", u"\u65e5\u2212\u672c.jp", kUnsafe},
    // Latin Small Letter Script G: ɡɡ.com
    {"xn--0naa.com", u"\u0261\u0261.com", kUnsafe},
    // Hangul Jamo(U+11xx)
    {"xn--0pdc3b.com", u"\u1102\u1103\u1110.com", kUnsafe},
    // degree sign: 36°c.com
    {"xn--36c-tfa.com", u"36\u00b0c.com", kUnsafe},
    // Pound sign
    {"xn--5free-fga.com", u"5free\u00a3.com", kUnsafe},
    // Hebrew points (U+05B0, U+05B6)
    {"xn--7cbl2kc2a.com", u"\u05e1\u05b6\u05e7\u05b0\u05e1.com", kUnsafe},
    // Danda(U+0964)
    {"xn--81bp1b6ch8s.com", u"\u0924\u093f\u091c\u0964\u0930\u0940.com",
     kUnsafe},
    // Small letter script G(U+0261)
    {"xn--oogle-qmc.com", u"\u0261oogle.com", kUnsafe},
    // Small Katakana Extension(U+31F1)
    {"xn--wlk.com", u"\u31f1.com", kUnsafe},
    // Heart symbol: ♥
    {"xn--ab-u0x.com", u"ab\u2665.com", kUnsafe},
    // Emoji
    {"xn--vi8hiv.xyz", u"\U0001f355\U0001f4a9.xyz", kUnsafe},
    // Registered trade mark
    {"xn--egistered-fna.com", u"\u00aeegistered.com", kUnsafe},
    // Latin Letter Retroflex Click
    {"xn--registered-25c.com", u"registered\u01c3.com", kUnsafe},
    // ASCII '!' not allowed in IDN
    {"xn--!-257eu42c.kr", u"\uc548\ub155!.kr", kUnsafe},
    // 'GOOGLE' in IPA extension: ɢᴏᴏɢʟᴇ
    {"xn--1naa7pn51hcbaa.com", u"\u0262\u1d0f\u1d0f\u0262\u029f\u1d07.com",
     kUnsafe},
    // Padlock icon spoof.
    {"xn--google-hj64e.com", u"\U0001f512google.com", kUnsafe},

    // Custom block list
    // Combining Long Solidus Overlay
    {"google.xn--comabc-k8d", u"google.com\u0338abc", kUnsafe},
    // Hyphenation Point instead of Katakana Middle dot
    {"xn--svgy16dha.jp", u"\u30a1\u2027\u30a3.jp", kUnsafe},
    // Gershayim with other Hebrew characters is allowed.
    {"xn--5db6bh9b.il", u"\u05e9\u05d1\u05f4\u05e6.il", kSafe},
    // Hebrew Gershayim with Latin is invalid according to Python's idna
    // package.
    {"xn--ab-yod.com", u"a\u05f4b.com", kInvalid},
    // Hebrew Gershayim with Arabic is disallowed.
    {"xn--5eb7h.eg", u"\u0628\u05f4.eg", kUnsafe},
#if BUILDFLAG(IS_APPLE)
    // These characters are blocked due to a font issue on Mac.
    // Tibetan transliteration characters.
    {"xn--com-lum.test.pl", u"com\u0f8c.test.pl", kUnsafe},
    // Arabic letter KASHMIRI YEH
    {"xn--fgb.com", u"\u0620.com", kUnsafe},
#endif

    // Hyphens (http://unicode.org/cldr/utility/confusables.jsp?a=-)
    // Hyphen-Minus (the only hyphen allowed)
    // abc-def
    {"abc-def.com", u"abc-def.com", kSafe},
    // Modifier Letter Minus Sign
    {"xn--abcdef-5od.com", u"abc\u02d7def.com", kUnsafe},
    // Hyphen
    {"xn--abcdef-dg0c.com", u"abc\u2010def.com", kUnsafe},
    // Non-Breaking Hyphen
    // This is actually an invalid IDNA domain (U+2011 normalizes to U+2010),
    // but it is included to ensure that we do not inadvertently allow this
    // character to be displayed as Unicode.
    {"xn--abcdef-kg0c.com", u"abc\u2011def.com", kInvalid},
    // Figure Dash.
    // Python's idna package refuses to decode the minus signs and dashes. ICU
    // decodes them but treats them as unsafe in spoof checks, so these test
    // cases are marked as unsafe instead of invalid.
    {"xn--abcdef-rg0c.com", u"abc\u2012def.com", kUnsafe},
    // En Dash
    {"xn--abcdef-yg0c.com", u"abc\u2013def.com", kUnsafe},
    // Hyphen Bullet
    {"xn--abcdef-kq0c.com", u"abc\u2043def.com", kUnsafe},
    // Minus Sign
    {"xn--abcdef-5d3c.com", u"abc\u2212def.com", kUnsafe},
    // Heavy Minus Sign
    {"xn--abcdef-kg1d.com", u"abc\u2796def.com", kUnsafe},
    // Em Dash
    // Small Em Dash (U+FE58) is normalized to Em Dash.
    {"xn--abcdef-5g0c.com", u"abc\u2014def.com", kUnsafe},
    // Coptic Small Letter Dialect-P Ni. Looks like dash.
    // Coptic Capital Letter Dialect-P Ni is normalized to small letter.
    {"xn--abcdef-yy8d.com", u"abc\u2cbbdef.com", kUnsafe},

    // Block NV8 (Not valid in IDN 2008) characters.
    // U+058A (֊)
    {"xn--ab-vfd.com", u"a\u058ab.com", kUnsafe},
    {"xn--y9ac3j.com", u"\u0561\u058a\u0562.com", kUnsafe},
    // U+2019 (’)
    {"xn--ab-n2t.com", u"a\u2019b.com", kUnsafe},
    // U+2027 (‧)
    {"xn--ab-u3t.com", u"a\u2027b.com", kUnsafe},
    // U+30A0 (゠)
    {"xn--ab-bg4a.com", u"a\u30a0b.com", kUnsafe},
    {"xn--9bk3828aea.com", u"\uac00\u30a0\uac01.com", kUnsafe},
    {"xn--9bk279fba.com", u"\u4e00\u30a0\u4e00.com", kUnsafe},
    {"xn--n8jl2x.com", u"\u304a\u30a0\u3044.com", kUnsafe},
    {"xn--fbke7f.com", u"\u3082\u30a0\u3084.com", kUnsafe},

    // Block single/double-quote-like characters.
    // U+02BB (ʻ)
    {"xn--ab-8nb.com", u"a\u02bbb.com", kUnsafe},
    // U+02BC (ʼ)
    {"xn--ab-cob.com", u"a\u02bcb.com", kUnsafe},
    // U+144A: Not allowed to mix with scripts other than Canadian Syllabics.
    {"xn--ab-jom.com", u"a\u144ab.com", kUnsafe},
    {"xn--xcec9s.com", u"\u1401\u144a\u1402.com", kUnsafe},

    // Custom dangerous patterns
    // Two Katakana-Hiragana combining mark in a row
    {"google.xn--com-oh4ba.evil.jp", u"google.com\u309a\u309a.evil.jp",
     kUnsafe},
    // Katakana Letter No not enclosed by {Han,Hiragana,Katakana}.
    {"google.xn--comevil-v04f.jp", u"google.com\u30ceevil.jp", kUnsafe},
    // TODO(jshin): Review the danger of allowing the following two.
    // Hiragana 'No' by itself is allowed.
    {"xn--ldk.jp", u"\u30ce.jp", kSafe},
    // Hebrew Gershayim used by itself is allowed.
    {"xn--5eb.il", u"\u05f4.il", kSafe},

    // Block RTL nonspacing marks (NSM) after unrelated scripts.
    {"xn--foog-ycg.com", u"foog\u0650.com", kUnsafe},    // Latin + Arabic NSM
    {"xn--foog-jdg.com", u"foog\u0654.com", kUnsafe},    // Latin + Arabic NSM
    {"xn--foog-jhg.com", u"foog\u0670.com", kUnsafe},    // Latin + Arbic NSM
    {"xn--foog-opf.com", u"foog\u05b4.com", kUnsafe},    // Latin + Hebrew NSM
    {"xn--shb5495f.com", u"\uac00\u0650.com", kUnsafe},  // Hang + Arabic NSM

    // Math Monospace Small A. When entered in Unicode, it's canonicalized to
    // 'a'. The punycode form should remain in punycode.
    {"xn--bc-9x80a.xyz", u"\U0001d68abc.xyz", kInvalid},
    // Math Sans Bold Capital Alpha
    {"xn--bc-rg90a.xyz", u"\U0001d756bc.xyz", kInvalid},
    // U+3000 is canonicalized to a space(U+0020), but the punycode form
    // should remain in punycode.
    {"xn--p6j412gn7f.cn", u"\u4e2d\u56fd\u3000", kInvalid},
    // U+3002 is canonicalized to ASCII fullstop(U+002E), but the punycode form
    // should remain in punycode.
    {"xn--r6j012gn7f.cn", u"\u4e2d\u56fd\u3002", kInvalid},
    // Invalid punycode
    // Has a codepoint beyond U+10FFFF.
    {"xn--krank-kg706554a", nullptr, kInvalid},
    // '?' in punycode.
    {"xn--hello?world.com", nullptr, kInvalid},

    // Not allowed in UTS46/IDNA 2008
    // Georgian Capital Letter(U+10BD)
    {"xn--1nd.com", u"\u10bd.com", kInvalid},
    // 3rd and 4th characters are '-'.
    {"xn-----8kci4dhsd", u"\u0440\u0443--\u0430\u0432\u0442\u043e", kInvalid},
    // Leading combining mark
    {"xn--72b.com", u"\u093e.com", kInvalid},
    // BiDi check per IDNA 2008/UTS 46
    // Cannot starts with AN(Arabic-Indic Number)
    {"xn--8hbae.eg", u"\u0662\u0660\u0660.eg", kInvalid},
    // Cannot start with a RTL character and ends with a LTR
    {"xn--x-ymcov.eg", u"\u062c\u0627\u0631x.eg", kInvalid},
    // Can start with a RTL character and ends with EN(European Number)
    {"xn--2-ymcov.eg", u"\u062c\u0627\u06312.eg", kSafe},
    // Can start with a RTL and end with AN
    {"xn--mgbjq0r.eg", u"\u062c\u0627\u0631\u0662.eg", kSafe},

    // Extremely rare Latin letters
    // Latin Ext B - Pinyin: ǔnion.com
    {"xn--nion-unb.com", u"\u01d4nion.com", kUnsafe},
    // Latin Ext C: ⱴase.com
    {"xn--ase-7z0b.com", u"\u2c74ase.com", kUnsafe},
    // Latin Ext D: ꝴode.com
    {"xn--ode-ut3l.com", u"\ua774ode.com", kUnsafe},
    // Latin Ext Additional: ḷily.com
    {"xn--ily-n3y.com", u"\u1e37ily.com", kUnsafe},
    // Latin Ext E: ꬺove.com
    {"xn--ove-8y6l.com", u"\uab3aove.com", kUnsafe},
    // Greek Ext: ᾳβγ.com
    {"xn--nxac616s.com", u"\u1fb3\u03b2\u03b3.com", kInvalid},
    // Cyrillic Ext A (label cannot begin with an illegal combining character).
    {"xn--lrj.com", u"\u2def.com", kInvalid},
    // Cyrillic Ext B: ꙡ.com
    {"xn--kx8a.com", u"\ua661.com", kUnsafe},
    // Cyrillic Ext C: ᲂ.com (Narrow o)
    {"xn--43f.com", u"\u1c82.com", kInvalid},

    // The skeleton of Extended Arabic-Indic Digit Zero (۰) is a dot. Check that
    // this is handled correctly (crbug/877045).
    {"xn--dmb", u"\u06f0", kSafe},

    // Test that top domains whose skeletons are the same as the domain name are
    // handled properly. In this case, tést.net should match test.net top
    // domain and not be converted to unicode.
    {"xn--tst-bma.net", u"t\u00e9st.net", kUnsafe},
    // Variations of the above, for testing crbug.com/925199.
    // some.tést.net should match test.net.
    {"some.xn--tst-bma.net", u"some.t\u00e9st.net", kUnsafe},
    // The following should not match test.net, so should be converted to
    // unicode.
    // ést.net (a suffix of tést.net).
    {"xn--st-9ia.net", u"\u00e9st.net", kSafe},
    // some.ést.net
    {"some.xn--st-9ia.net", u"some.\u00e9st.net", kSafe},
    // atést.net (tést.net is a suffix of atést.net)
    {"xn--atst-cpa.net", u"at\u00e9st.net", kSafe},
    // some.atést.net
    {"some.xn--atst-cpa.net", u"some.at\u00e9st.net", kSafe},

    // Modifier-letter-voicing should be blocked (wwwˬtest.com).
    {"xn--wwwtest-2be.com", u"www\u02ectest.com", kUnsafe},

    // oĸ.com: Not a top domain, should be blocked because of Kra.
    {"xn--o-tka.com", u"o\u0138.com", kUnsafe},

    // U+4E00 and U+3127 should be blocked when next to non-CJK.
    {"xn--ipaddress-w75n.com", u"ip\u4e00address.com", kUnsafe},
    {"xn--ipaddress-wx5h.com", u"ip\u3127address.com", kUnsafe},
    // U+4E00 and U+3127 at the beginning and end of a string.
    {"xn--google-gg5e.com", u"google\u3127.com", kUnsafe},
    {"xn--google-9f5e.com", u"\u3127google.com", kUnsafe},
    {"xn--google-gn7i.com", u"google\u4e00.com", kUnsafe},
    {"xn--google-9m7i.com", u"\u4e00google.com", kUnsafe},
    // These are allowed because U+4E00 and U+3127 are not immediately next to
    // non-CJK.
    {"xn--gamer-fg1hz05u.com", u"\u4e00\u751fgamer.com", kSafe},
    {"xn--gamer-kg1hy05u.com", u"gamer\u751f\u4e00.com", kSafe},
    {"xn--gamer-f94d4426b.com", u"\u3127\u751fgamer.com", kSafe},
    {"xn--gamer-k94d3426b.com", u"gamer\u751f\u3127.com", kSafe},
    {"xn--4gqz91g.com", u"\u4e00\u732b.com", kSafe},
    {"xn--4fkv10r.com", u"\u3127\u732b.com", kSafe},
    // U+4E00 with another ideograph.
    {"xn--4gqc.com", u"\u4e00\u4e01.com", kSafe},

    // CJK ideographs looking like slashes should be blocked when next to
    // non-CJK.
    {"example.xn--comtest-k63k", u"example.com\u4e36test", kUnsafe},
    {"example.xn--comtest-u83k", u"example.com\u4e40test", kUnsafe},
    {"example.xn--comtest-283k", u"example.com\u4e41test", kUnsafe},
    {"example.xn--comtest-m83k", u"example.com\u4e3ftest", kUnsafe},
    // This is allowed because the ideographs are not immediately next to
    // non-CJK.
    {"xn--oiqsace.com", u"\u4e36\u4e40\u4e41\u4e3f.com", kSafe},

    // Kana voiced sound marks are not allowed.
    {"xn--google-1m4e.com", u"google\u3099.com", kUnsafe},
    {"xn--google-8m4e.com", u"google\u309A.com", kUnsafe},

    // Small letter theta looks like a zero.
    {"xn--123456789-yzg.com", u"123456789\u03b8.com", kUnsafe},

    {"xn--est-118d.net", u"\u4e03est.net", kUnsafe},
    {"xn--est-918d.net", u"\u4e05est.net", kUnsafe},
    {"xn--est-e28d.net", u"\u4e06est.net", kUnsafe},
    {"xn--est-t18d.net", u"\u4e01est.net", kUnsafe},
    {"xn--3-cq6a.com", u"\u4e293.com", kUnsafe},
    {"xn--cxe-n68d.com", u"c\u4e2bxe.com", kUnsafe},
    {"xn--cye-b98d.com", u"cy\u4e42e.com", kUnsafe},

    // U+05D7 can look like Latin n in many fonts.
    {"xn--ceba.com", u"\u05d7\u05d7.com", kUnsafe},

    // U+00FE (þ) and U+00F0 (ð) are only allowed under the .is and .fo TLDs.
    {"xn--acdef-wva.com", u"a\u00fecdef.com", kUnsafe},
    {"xn--mnpqr-jta.com", u"mn\u00f0pqr.com", kUnsafe},
    {"xn--acdef-wva.is", u"a\u00fecdef.is", kSafe},
    {"xn--mnpqr-jta.is", u"mn\u00f0pqr.is", kSafe},
    {"xn--mnpqr-jta.fo", u"mn\u00f0pqr.fo", kSafe},

    // U+0259 (ə) is only allowed under the .az TLD.
    {"xn--xample-vyc.com", u"\u0259xample.com", kUnsafe},
    {"xn--xample-vyc.az", u"\u0259xample.az", kSafe},

    // U+00B7 is only allowed on Catalan domains between two l's.
    {"xn--googlecom-5pa.com", u"google\u00b7com.com", kUnsafe},
    {"xn--ll-0ea.com", u"l\u00b7l.com", kUnsafe},
    {"xn--ll-0ea.cat", u"l\u00b7l.cat", kSafe},
    {"xn--al-0ea.cat", u"a\u00b7l.cat", kUnsafe},
    {"xn--la-0ea.cat", u"l\u00b7a.cat", kUnsafe},
    {"xn--l-fda.cat", u"\u00b7l.cat", kUnsafe},
    {"xn--l-gda.cat", u"l\u00b7.cat", kUnsafe},

    {"xn--googlecom-gk6n.com", u"google\u4e28com.com", kUnsafe},
    {"xn--googlecom-0y6n.com", u"google\u4e5bcom.com", kUnsafe},
    {"xn--googlecom-v85n.com", u"google\u4e03com.com", kUnsafe},
    {"xn--googlecom-g95n.com", u"google\u4e05com.com", kUnsafe},
    {"xn--googlecom-go6n.com", u"google\u4e36com.com", kUnsafe},
    {"xn--googlecom-b76o.com", u"google\u5341com.com", kUnsafe},
    {"xn--googlecom-ql3h.com", u"google\u3007com.com", kUnsafe},
    {"xn--googlecom-0r5h.com", u"google\u3112com.com", kUnsafe},
    {"xn--googlecom-bu5h.com", u"google\u311acom.com", kUnsafe},
    {"xn--googlecom-qv5h.com", u"google\u311fcom.com", kUnsafe},
    {"xn--googlecom-0x5h.com", u"google\u3127com.com", kUnsafe},
    {"xn--googlecom-by5h.com", u"google\u3128com.com", kUnsafe},
    {"xn--googlecom-ly5h.com", u"google\u3129com.com", kUnsafe},
    {"xn--googlecom-5o5h.com", u"google\u3108com.com", kUnsafe},
    {"xn--googlecom-075n.com", u"google\u4e00com.com", kUnsafe},
    {"xn--googlecom-046h.com", u"google\u31bacom.com", kUnsafe},
    {"xn--googlecom-026h.com", u"google\u31b3com.com", kUnsafe},
    {"xn--googlecom-lg9q.com", u"google\u5de5com.com", kUnsafe},
    {"xn--googlecom-g040a.com", u"google\u8ba0com.com", kUnsafe},
    {"xn--googlecom-b85n.com", u"google\u4e01com.com", kUnsafe},

    // Whole-script-confusables. Cyrillic is sufficiently handled in cases above
    // so it's not included here.
    // Armenian:
    {"xn--mbbkpm.com", u"\u0578\u057d\u0582\u0585.com", kUnsafe},
    {"xn--mbbkpm.am", u"\u0578\u057d\u0582\u0585.am", kSafe},
    {"xn--mbbkpm.xn--y9a3aq", u"\u0578\u057d\u0582\u0585.\u0570\u0561\u0575",
     kSafe},
    // Ethiopic:
    {"xn--6xd66aa62c.com", u"\u1220\u12d0\u12d0\u1350.com", kUnsafe},
    {"xn--6xd66aa62c.et", u"\u1220\u12d0\u12d0\u1350.et", kSafe},
    {"xn--6xd66aa62c.xn--m0d3gwjla96a",
     u"\u1220\u12d0\u12d0\u1350.\u12a2\u1275\u12ee\u1335\u12eb", kSafe},
    // Greek:
    {"xn--mxapd.com", u"\u03b9\u03ba\u03b1.com", kUnsafe},
    {"xn--mxapd.gr", u"\u03b9\u03ba\u03b1.gr", kSafe},
    {"xn--mxapd.xn--qxam", u"\u03b9\u03ba\u03b1.\u03b5\u03bb", kSafe},
    // Georgian:
    {"xn--gpd3ag.com", u"\u10fd\u10ff\u10ee.com", kUnsafe},
    {"xn--gpd3ag.ge", u"\u10fd\u10ff\u10ee.ge", kSafe},
    {"xn--gpd3ag.xn--node", u"\u10fd\u10ff\u10ee.\u10d2\u10d4", kSafe},
    // Hebrew:
    {"xn--7dbh4a.com", u"\u05d7\u05e1\u05d3.com", kUnsafe},
    {"xn--7dbh4a.il", u"\u05d7\u05e1\u05d3.il", kSafe},
    {"xn--9dbq2a.xn--7dbh4a", u"\u05e7\u05d5\u05dd.\u05d7\u05e1\u05d3", kSafe},
    // Myanmar:
    {"xn--oidbbf41a.com", u"\u1004\u1040\u1002\u1001\u1002.com", kUnsafe},
    {"xn--oidbbf41a.mm", u"\u1004\u1040\u1002\u1001\u1002.mm", kSafe},
    {"xn--oidbbf41a.xn--7idjb0f4ck",
     u"\u1004\u1040\u1002\u1001\u1002.\u1019\u103c\u1014\u103a\u1019\u102c",
     kSafe},
    // Myanmar Shan digits:
    {"xn--rmdcmef.com", u"\u1090\u1091\u1095\u1096\u1097.com", kUnsafe},
    {"xn--rmdcmef.mm", u"\u1090\u1091\u1095\u1096\u1097.mm", kSafe},
    {"xn--rmdcmef.xn--7idjb0f4ck",
     u"\u1090\u1091\u1095\u1096\u1097.\u1019\u103c\u1014\u103a\u1019\u102c",
     kSafe},
// Thai:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    {"xn--o3cedqz2c.com", u"\u0e17\u0e19\u0e1a\u0e1e\u0e23\u0e2b.com", kUnsafe},
    {"xn--o3cedqz2c.th", u"\u0e17\u0e19\u0e1a\u0e1e\u0e23\u0e2b.th", kSafe},
    {"xn--o3cedqz2c.xn--o3cw4h",
     u"\u0e17\u0e19\u0e1a\u0e1e\u0e23\u0e2b.\u0e44\u0e17\u0e22", kSafe},
#else
    {"xn--r3ch7hsc.com", u"\u0e1e\u0e1a\u0e40\u0e50.com", kUnsafe},
    {"xn--r3ch7hsc.th", u"\u0e1e\u0e1a\u0e40\u0e50.th", kSafe},
    {"xn--r3ch7hsc.xn--o3cw4h", u"\u0e1e\u0e1a\u0e40\u0e50.\u0e44\u0e17\u0e22",
     kSafe},
#endif

    // Indic scripts:
    // Bengali:
    {"xn--07baub.com", u"\u09e6\u09ed\u09e6\u09ed.com", kUnsafe},
    // Devanagari:
    {"xn--62ba6j.com", u"\u093d\u0966\u093d.com", kUnsafe},
    // Gujarati:
    {"xn--becd.com", u"\u0aa1\u0a9f.com", kUnsafe},
    // Gurmukhi:
    {"xn--occacb.com", u"\u0a66\u0a67\u0a66\u0a67.com", kUnsafe},
    // Kannada:
    {"xn--stca6jf.com", u"\u0cbd\u0ce6\u0cbd\u0ce7.com", kUnsafe},
    // Malayalam:
    {"xn--lwccv.com", u"\u0d1f\u0d20\u0d27.com", kUnsafe},
    // Oriya:
    {"xn--zhca6ub.com", u"\u0b6e\u0b20\u0b6e\u0b20.com", kUnsafe},
    // Tamil:
    {"xn--mlca6ab.com", u"\u0b9f\u0baa\u0b9f\u0baa.com", kUnsafe},
    // Telugu:
    {"xn--brcaabbb.com", u"\u0c67\u0c66\u0c67\u0c66\u0c67\u0c66.com", kUnsafe},

    // IDN domain matching an IDN top-domain (f\u00f3\u00f3.com)
    {"xn--fo-5ja.com", u"f\u00f3o.com", kUnsafe},

    // crbug.com/769547: Subdomains of top domains should be allowed.
    {"xn--xample-9ua.test.net", u"\u00e9xample.test.net", kSafe},
    // Skeleton of the eTLD+1 matches a top domain, but the eTLD+1 itself is
    // not a top domain. Should not be decoded to unicode.
    {"xn--xample-9ua.test.xn--nt-bja", u"\u00e9xample.test.n\u00e9t", kUnsafe},

    // Digit lookalike check of 16კ.com with character “კ” (U+10D9)
    // Test case for https://crbug.com/1156531
    {"xn--16-1ik.com", u"16\u10d9.com", kUnsafe},

    // Skeleton generator check of officeკ65.com with character “კ” (U+10D9)
    // Test case for https://crbug.com/1156531
    {"xn--office65-l04a.com", u"office\u10d965.com", kUnsafe},

    // Digit lookalike check of 16ੜ.com with character “ੜ” (U+0A5C)
    // Test case for https://crbug.com/1156531 (missed skeleton map)
    {"xn--16-ogg.com", u"16\u0a5c.com", kUnsafe},

    // Skeleton generator check of officeੜ65.com with character “ੜ” (U+0A5C)
    // Test case for https://crbug.com/1156531 (missed skeleton map)
    {"xn--office65-hts.com", u"office\u0a5c65.com", kUnsafe},

    // New test cases go ↑↑ above.

    // /!\ WARNING: You MUST use tools/security/idn_test_case_generator.py to
    // generate new test cases, as specified by the comment at the top of this
    // test list. Why must you use that python script?
    // 1. It is easy to get things wrong. There were several hand-crafted
    //    incorrect test cases committed that was later fixed.
    // 2. This test _also_ is a test of Chromium's IDN encoder/decoder, so using
    //    Chromium's IDN encoder/decoder to generate test files loses an
    //    advantage of having Python's IDN encode/decode the tests.
};

namespace test {
#include "components/url_formatter/spoof_checks/top_domains/idn_test_domains-trie-inc.cc"
}

bool IsPunycode(const std::u16string& s) {
  return s.size() > 4 && s[0] == L'x' && s[1] == L'n' && s[2] == L'-' &&
         s[3] == L'-';
}

}  // namespace

// IDNA mode to use in tests.
enum class IDNAMode { kTransitional, kNonTransitional };

class IDNSpoofCheckerTest : public ::testing::Test,
                            public ::testing::WithParamInterface<IDNAMode> {
 protected:
  void SetUp() override {
    if (GetParam() == IDNAMode::kNonTransitional) {
      scoped_feature_list_.InitAndEnableFeature(
          url::kUseIDNA2008NonTransitional);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          url::kUseIDNA2008NonTransitional);
    }
    IDNSpoofChecker::HuffmanTrieParams trie_params{
        test::kTopDomainsHuffmanTree, sizeof(test::kTopDomainsHuffmanTree),
        test::kTopDomainsTrie, test::kTopDomainsTrieBits,
        test::kTopDomainsRootPosition};
    IDNSpoofChecker::SetTrieParamsForTesting(trie_params);
  }

  void TearDown() override { IDNSpoofChecker::RestoreTrieParamsForTesting(); }

  void RunIDNToUnicodeTest(const IDNTestCase& test) {
    // Sanity check to ensure that the unicode output matches the input. Bypass
    // all spoof checks by doing an unsafe conversion.
    const IDNConversionResult unsafe_result =
        UnsafeIDNToUnicodeWithDetails(test.input);

    // Ignore inputs that can't be converted even with unsafe conversion because
    // they contain certain characters not allowed in IDNs. E.g. U+24DF (Latin
    // CIRCLED LATIN SMALL LETTER P) in hostname causes the conversion to fail
    // before reaching spoof checks.
    if (test.expected_result != kInvalid) {
      // Also ignore domains that need to remain partially in punycode, such
      // as ѕсоре-рау.한국 where scope-pay is a Cyrillic whole-script
      // confusable but 한국 is safe. This would require adding yet another
      // field to the the test struct.
      if (!IsPunycode(test.unicode_output)) {
        EXPECT_EQ(unsafe_result.result, test.unicode_output);
      }
    } else {
      // Invalid punycode should not be converted.
      EXPECT_EQ(unsafe_result.result, base::ASCIIToUTF16(test.input));
    }

    const std::u16string output(IDNToUnicode(test.input));
    const std::u16string expected(test.expected_result == kSafe
                                      ? test.unicode_output
                                      : base::ASCIIToUTF16(test.input));
    EXPECT_EQ(expected, output);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         IDNSpoofCheckerTest,
                         ::testing::Values(IDNAMode::kTransitional,
                                           IDNAMode::kNonTransitional));

// Test that a domain entered as punycode is decoded to unicode if safe,
// otherwise is left in punycode.
//
// TODO(crbug.com/40664864): This should also check if a domain entered as
// unicode is properly decoded or not-decoded. This is important in cases where
// certain unicode characters are canonicalized to other characters.
// E.g. Mathematical Monospace Small A (U+1D68A) is canonicalized to "a" when
// used in a domain name.
TEST_P(IDNSpoofCheckerTest, IDNToUnicode) {
  for (const auto& test : kIdnCases) {
    RunIDNToUnicodeTest(test);
  }
}

// Same as IDNToUnicode but only tests hostnames with deviation characters.
TEST_P(IDNSpoofCheckerTest, IDNToUnicodeDeviationCharacters) {
  // Tests for 4 Deviation characters between IDNA 2003 and IDNA 2008. When
  // entered in Unicode:
  // - In Transitional mode, sharp-s and final-sigma are mapped to 'ss' and
  //   sigma and ZWJ and ZWNJ two are mapped away. However, the punycode form
  //   should remain in punycode.
  // - In Non-Transitional mode, sharp-s and final-sigma shouldn't be be mapped
  //   and hostnames containing them should be considered safe. ZWJ and ZWNJ
  //   should still be considered unsafe.
  bool is_non_transitional_idna = GetParam() == IDNAMode::kNonTransitional;

  const IDNTestCase kTestCases[] = {
      // U+00DF(sharp-s)
      {"xn--fu-hia.de", u"fu\u00df.de",
       is_non_transitional_idna ? kSafe : kUnsafe},
      // U+03C2(final-sigma)
      {"xn--mxac2c.gr", u"\u03b1\u03b2\u03c2.gr",
       is_non_transitional_idna ? kSafe : kUnsafe},

      // Treat ZWJ and ZWNJ explicitly unsafe, even in Non-Transitional mode.
      // U+200C(ZWNJ)
      {"xn--h2by8byc123p.in", u"\u0924\u094d\u200c\u0930\u093f.in", kUnsafe},
      // U+200C(ZWJ)
      {"xn--11b6iy14e.in", u"\u0915\u094d\u200d.in", kUnsafe},

      // youtuße.com is always unsafe:
      // - In Transitional mode, deviation characters are disallowed.
      // - In Non-Transitional mode, skeleton of youtuße.com matches
      //   youtube.com which is a test top domain.
      {"xn--youtue-fta.com", u"youtu\u00dfe.com", kUnsafe}};
  for (const auto& test : kTestCases) {
    RunIDNToUnicodeTest(test);
  }
}

TEST_P(IDNSpoofCheckerTest, GetSimilarTopDomain) {
  struct TestCase {
    const char16_t* const hostname;
    const char* const expected_top_domain;
  } kTestCases[] = {
      {u"tést.net", "test.net"},
      {u"subdomain.tést.net", "test.net"},
      // A top domain should not return a similar top domain result.
      {u"test.net", ""},
      // A subdomain of a top domain should not return a similar top domain
      // result.
      {u"subdomain.test.net", ""},
      // An IDN subdomain of a top domain should not return a similar top domain
      // result.
      {u"subdómain.test.net", ""},
      // Test cases for https://crbug.com/1250993:
      {u"tesł.net", "test.net"},
      {u"łest.net", "test.net"},
      {u"łesł.net", "test.net"},
      // Test case for https://crbug.com/1207187
      {u"စ2.com", "o2.com"},
      // Test case for https://crbug.com/1156531
      {u"კ9.com", "39.com"},
      // Test case for https://crbug.com/1156531 (missed skeleton map)
      {u"ੜ9.com", "39.com"}};
  for (const TestCase& test_case : kTestCases) {
    const TopDomainEntry entry =
        IDNSpoofChecker().GetSimilarTopDomain(test_case.hostname);
    EXPECT_EQ(test_case.expected_top_domain, entry.domain);
    EXPECT_FALSE(entry.is_top_bucket);
  }
}

TEST_P(IDNSpoofCheckerTest, LookupSkeletonInTopDomains) {
  {
    TopDomainEntry entry =
        IDNSpoofChecker().LookupSkeletonInTopDomains("d4OOO.corn");
    EXPECT_EQ("d4000.com", entry.domain);
    EXPECT_TRUE(entry.is_top_bucket);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kFull);
  }
  {
    TopDomainEntry entry = IDNSpoofChecker().LookupSkeletonInTopDomains(
        "d4OOOcorn", SkeletonType::kSeparatorsRemoved);
    EXPECT_EQ("d4000.com", entry.domain);
    EXPECT_TRUE(entry.is_top_bucket);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kSeparatorsRemoved);
  }
  {
    TopDomainEntry entry =
        IDNSpoofChecker().LookupSkeletonInTopDomains("digklrno68.corn");
    EXPECT_EQ("digklmo68.com", entry.domain);
    EXPECT_FALSE(entry.is_top_bucket);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kFull);
  }
}

// Same test as LookupSkeletonInTopDomains but using the real top domain list.
TEST(IDNSpoofCheckerNoFixtureTest, LookupSkeletonInTopDomains) {
  {
    TopDomainEntry entry =
        IDNSpoofChecker().LookupSkeletonInTopDomains("google.corn");
    EXPECT_EQ("google.com", entry.domain);
    EXPECT_TRUE(entry.is_top_bucket);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kFull);
  }
  {
    TopDomainEntry entry = IDNSpoofChecker().LookupSkeletonInTopDomains(
        "googlecorn", SkeletonType::kSeparatorsRemoved);
    EXPECT_EQ("google.com", entry.domain);
    EXPECT_TRUE(entry.is_top_bucket);
    EXPECT_EQ(entry.skeleton_type, SkeletonType::kSeparatorsRemoved);
  }
  {
    // This is data dependent, must be updated when the top domain list
    // is updated.
    TopDomainEntry entry =
        IDNSpoofChecker().LookupSkeletonInTopDomains("google.sk");
    EXPECT_EQ("google.sk", entry.domain);
    EXPECT_FALSE(entry.is_top_bucket);
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
    const char16_t* const expected_unicode;
    // Whether the input (punycode) has idn.
    const bool expected_has_idn;
    // The top domain that |punycode| matched to, if any.
    const char* const expected_matching_domain;
    // If true, the matching top domain is expected to be in top 500.
    const bool expected_is_top_bucket;
    const IDNSpoofChecker::Result expected_spoof_check_result;
  } kTestCases[] = {
      {// An ASCII, top domain.
       "google.com", u"google.com", false,
       // Since it's not unicode, we won't attempt to match it to a top domain.
       "",
       // ...And since we don't match it to a top domain, we don't know if it's
       // a top 500 domain.
       false, IDNSpoofChecker::Result::kNone},
      {// An ASCII domain that's not a top domain.
       "not-top-domain.com", u"not-top-domain.com", false, "", false,
       IDNSpoofChecker::Result::kNone},
      {// A unicode domain that's valid according to all of the rules in IDN
       // spoof checker except that it matches a top domain. Should be
       // converted to punycode. Spoof check result is kSafe because top domain
       // similarity isn't included in IDNSpoofChecker::Result.
       "xn--googl-fsa.com", u"googlé.com", true, "google.com", true,
       IDNSpoofChecker::Result::kSafe},
      {// A unicode domain that's not valid according to the rules in IDN spoof
       // checker (whole script confusable in Cyrillic) and it matches a top
       // domain. Should be converted to punycode.
       "xn--80ak6aa92e.com", u"аррӏе.com", true, "apple.com", true,
       IDNSpoofChecker::Result::kWholeScriptConfusable},
      {// A unicode domain that's not valid according to the rules in IDN spoof
       // checker (mixed script) but it doesn't match a top domain.
       "xn--o-o-oai-26a223aia177a7ab7649d.com", u"ɴoτ-τoρ-ďoᛖaiɴ.com", true, "",
       false, IDNSpoofChecker::Result::kICUSpoofChecks}};

  for (const TestCase& test_case : kTestCases) {
    const url_formatter::IDNConversionResult result =
        UnsafeIDNToUnicodeWithDetails(test_case.punycode);
    EXPECT_EQ(test_case.expected_unicode, result.result);
    EXPECT_EQ(test_case.expected_has_idn, result.has_idn_component);
    EXPECT_EQ(test_case.expected_matching_domain,
              result.matching_top_domain.domain);
    EXPECT_EQ(test_case.expected_is_top_bucket,
              result.matching_top_domain.is_top_bucket);
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
  const GURL url1("http://appӏe.com");
  const url_formatter::IDNConversionResult result1 =
      UnsafeIDNToUnicodeWithDetails(url1.host());
  Skeletons skeletons1 = checker.GetSkeletons(result1.result);
  EXPECT_EQ(Skeletons({"apple.corn", "appie.corn"}), skeletons1);

  const GURL url2("http://œxamþle.com");
  const url_formatter::IDNConversionResult result2 =
      UnsafeIDNToUnicodeWithDetails(url2.host());
  Skeletons skeletons2 = checker.GetSkeletons(result2.result);
  // This skeleton set doesn't include strings with "œ" because it gets
  // converted to "oe" by ICU during skeleton extraction.
  EXPECT_EQ(Skeletons({"oexarnþle.corn", "oexarnple.corn", "oexarnble.corn",
                       "cexarnþle.corn", "cexarnple.corn", "cexarnble.corn"}),
            skeletons2);
}

TEST(IDNSpoofCheckerNoFixtureTest, AlternativeSkeletons) {
  struct TestCase {
    // String whose alternative strings will be generated
    std::u16string input;
    // Maximum number of alternative strings to generate.
    size_t max_alternatives;
    // Expected string set.
    base::flat_set<std::u16string> expected_strings;
  } kTestCases[] = {
      {u"", 0, {}},
      {u"", 1, {}},
      {u"", 2, {}},
      {u"", 100, {}},

      {u"a", 0, {}},
      {u"a", 1, {u"a"}},
      {u"a", 2, {u"a"}},
      {u"a", 100, {u"a"}},

      {u"ab", 0, {}},
      {u"ab", 1, {u"ab"}},
      {u"ab", 2, {u"ab"}},
      {u"ab", 100, {u"ab"}},

      {u"œ", 0, {}},
      {u"œ", 1, {u"œ"}},
      {u"œ", 2, {u"œ", u"ce"}},
      {u"œ", 100, {u"œ", u"ce", u"oe"}},

      {u"œxample", 0, {}},
      {u"œxample", 1, {u"œxample"}},
      {u"œxample", 2, {u"œxample", u"cexample"}},
      {u"œxample", 100, {u"œxample", u"cexample", u"oexample"}},

      {u"œxamþle", 0, {}},
      {u"œxamþle", 1, {u"œxamþle"}},
      {u"œxamþle", 2, {u"œxamþle", u"œxamble"}},
      {u"œxamþle",
       100,
       {u"œxamþle", u"œxample", u"œxamble", u"oexamþle", u"oexample",
        u"oexamble", u"cexamþle", u"cexample", u"cexamble"}},

      // Strings with many multi-character skeletons shouldn't generate any
      // supplemental hostnames.
      {u"œœœœœœœœœœœœœœœœœœœœœœœœœœœœœœ", 0, {}},
      {u"œœœœœœœœœœœœœœœœœœœœœœœœœœœœœœ", 1, {}},
      {u"œœœœœœœœœœœœœœœœœœœœœœœœœœœœœœ", 2, {}},
      {u"œœœœœœœœœœœœœœœœœœœœœœœœœœœœœœ", 100, {}},

      {u"łwiłłer", 0, {}},
      {u"łwiłłer", 1, {u"łwiłłer"}},
      {u"łwiłłer",
       2,
       {u"\x142wi\x142ler",
        u"\x142wi\x142\x142"
        u"er"}},
      {u"łwiłłer",
       100,
       {u"lwiller",
        u"lwilter",
        u"lwil\x142"
        u"er",
        u"lwitler",
        u"lwitter",
        u"lwit\x142"
        u"er",
        u"lwi\x142ler",
        u"lwi\x142ter",
        u"lwi\x142\x142"
        u"er",
        u"twiller",
        u"twilter",
        u"twil\x142"
        u"er",
        u"twitler",
        u"twitter",
        u"twit\x142"
        u"er",
        u"twi\x142ler",
        u"twi\x142ter",
        u"twi\x142\x142"
        u"er",
        u"\x142willer",
        u"\x142wilter",
        u"\x142wil\x142"
        u"er",
        u"\x142witler",
        u"\x142witter",
        u"\x142wit\x142"
        u"er",
        u"\x142wi\x142ler",
        u"\x142wi\x142ter",
        u"\x142wi\x142\x142"
        u"er"}},
  };
  SkeletonMap skeleton_map;
  skeleton_map[u'œ'] = {"ce", "oe"};
  skeleton_map[u'þ'] = {"b", "p"};
  skeleton_map[u'ł'] = {"l", "t"};

  for (const TestCase& test_case : kTestCases) {
    const auto strings = SkeletonGenerator::GenerateSupplementalHostnames(
        test_case.input, test_case.max_alternatives, skeleton_map);
    EXPECT_LE(strings.size(), test_case.max_alternatives);
    EXPECT_EQ(strings, test_case.expected_strings);
  }
}

TEST(IDNSpoofCheckerNoFixtureTest, MaybeRemoveDiacritics) {
  // Latin-Greek-Cyrillic example. Diacritic should be removed.
  IDNSpoofChecker checker;
  const GURL url("http://éxample.com");
  const url_formatter::IDNConversionResult result =
      UnsafeIDNToUnicodeWithDetails(url.host());
  std::u16string diacritics_removed =
      checker.MaybeRemoveDiacritics(result.result);
  EXPECT_EQ(u"example.com", diacritics_removed);

  // Non-LGC example, diacritic shouldn't be removed. The hostname
  // will be marked as unsafe by the spoof checks anyways, so diacritic
  // removal isn't necessary.
  const GURL non_lgc_url("http://xn--lsa922apb7a6do.com");
  const url_formatter::IDNConversionResult non_lgc_result =
      UnsafeIDNToUnicodeWithDetails(non_lgc_url.host());
  std::u16string diacritics_not_removed =
      checker.MaybeRemoveDiacritics(non_lgc_result.result);
  EXPECT_EQ(u"नागरी́.com", diacritics_not_removed);
  EXPECT_EQ(IDNSpoofChecker::Result::kDangerousPattern,
            non_lgc_result.spoof_check_result);
}

TEST(IDNSpoofCheckerNoFixtureTest, GetDeviationCharacter) {
  IDNSpoofChecker checker;
  EXPECT_EQ(IDNA2008DeviationCharacter::kNone,
            checker.GetDeviationCharacter(u"example.com"));
  // These test cases are from
  // https://www.unicode.org/reports/tr46/tr46-27.html#Table_Deviation_Characters.
  // faß.de:
  EXPECT_EQ(IDNA2008DeviationCharacter::kEszett,
            checker.GetDeviationCharacter(u"fa\u00df.de"));
  // βόλος.com:
  EXPECT_EQ(
      IDNA2008DeviationCharacter::kGreekFinalSigma,
      checker.GetDeviationCharacter(u"\u03b2\u03cc\u03bb\u03bf\u03c2.com"));
  // ශ්‍රී.com:
  EXPECT_EQ(
      IDNA2008DeviationCharacter::kZeroWidthJoiner,
      checker.GetDeviationCharacter(u"\u0dc1\u0dca\u200d\u0dbb\u0dd3.com"));
  // نامه<ZWNJ>ای.com:
  EXPECT_EQ(IDNA2008DeviationCharacter::kZeroWidthNonJoiner,
            checker.GetDeviationCharacter(
                u"\u0646\u0627\u0645\u0647\u200c\u0627\u06cc.com"));
}

}  // namespace url_formatter

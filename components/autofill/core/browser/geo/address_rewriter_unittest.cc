// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/address_rewriter.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using autofill::AddressRewriter;

TEST(AddressRewriterTest, InvalidCountryCode) {
  AddressRewriter ad =
      AddressRewriter::ForCountryCode(AddressCountryCode("ZZZZ"));
  const std::u16string kSomeRandomText = u"some random text";
  const std::u16string kOtherRandomText = u"other random text";

  EXPECT_EQ(ad.Rewrite(kSomeRandomText), ad.Rewrite(kSomeRandomText));
  EXPECT_EQ(ad.Rewrite(kOtherRandomText), ad.Rewrite(kOtherRandomText));

  EXPECT_NE(ad.Rewrite(kSomeRandomText), ad.Rewrite(kOtherRandomText));
}

TEST(AddressRewriterTest, LastRule) {
  AddressRewriter last_rule = AddressRewriter::ForCustomRules("1\t2\n3\t4\n");
  AddressRewriter large_rewrite =
      AddressRewriter::ForCustomRules("1\tonelongrewrite\n2\tshort\n");

  EXPECT_EQ(last_rule.Rewrite(u"3"), last_rule.Rewrite(u"4"));
  // Checks if last rule works when previous rewrite is larger than last rule.
  EXPECT_EQ(large_rewrite.Rewrite(u"2"), large_rewrite.Rewrite(u"short"));
}

TEST(AddressRewriterTest, AD) {
  AddressRewriter ad =
      AddressRewriter::ForCountryCode(AddressCountryCode("ad"));
  EXPECT_EQ(ad.Rewrite(u"parroquia de andorra la vella"),
            ad.Rewrite(u"andorra la vella"));
  EXPECT_EQ(ad.Rewrite(u"principal de andorra"), ad.Rewrite(u"an"));
  EXPECT_EQ(ad.Rewrite(u"or"), ad.Rewrite(u"ordino"));
}

TEST(AddressRewriterTest, AR) {
  AddressRewriter ar =
      AddressRewriter::ForCountryCode(AddressCountryCode("ar"));
  EXPECT_EQ(ar.Rewrite(u"tierra del fuego antartida e islas del atlantico sur"),
            ar.Rewrite(u"tierra del fuego"));
  EXPECT_EQ(ar.Rewrite(u"ciudad autonoma de buenos aires"),
            ar.Rewrite(u"capital federal"));
}

TEST(AddressRewriterTest, AU) {
  AddressRewriter au =
      AddressRewriter::ForCountryCode(AddressCountryCode("au"));
  EXPECT_EQ(au.Rewrite(u"australian capital territory"), au.Rewrite(u"act"));
  EXPECT_EQ(au.Rewrite(u"jervis bay territory"), au.Rewrite(u"jbt"));
}

TEST(AddressRewriterTest, BE) {
  AddressRewriter be =
      AddressRewriter::ForCountryCode(AddressCountryCode("be"));
  EXPECT_EQ(be.Rewrite(u"brussels hoofdstedelijk gewest"),
            be.Rewrite(u"region de bruxelles capitale"));
  EXPECT_EQ(be.Rewrite(u"arrondissement administratif de foo"),
            be.Rewrite(u"foo"));
}

TEST(AddressRewriterTest, BR) {
  AddressRewriter br =
      AddressRewriter::ForCountryCode(AddressCountryCode("br"));
  EXPECT_EQ(br.Rewrite(u"rio grande do norte"), br.Rewrite(u"rn"));
}

TEST(AddressRewriterTest, CA) {
  AddressRewriter ca =
      AddressRewriter::ForCountryCode(AddressCountryCode("ca"));
  EXPECT_EQ(ca.Rewrite(u"qc"), ca.Rewrite(u"quebec"));
  EXPECT_EQ(ca.Rewrite(u"prince edward island"), ca.Rewrite(u"pei"));
  EXPECT_EQ(ca.Rewrite(u"prince edward island"),
            ca.Rewrite(u"ile du prince edouard"));
  EXPECT_EQ(ca.Rewrite(u"cul-de-sac"), ca.Rewrite(u"cul de sac"));
  EXPECT_EQ(ca.Rewrite(u"st"), ca.Rewrite(u"street"));
  EXPECT_EQ(ca.Rewrite(u"sainte"), ca.Rewrite(u"saint"));
}

TEST(AddressRewriterTest, CH) {
  AddressRewriter ch =
      AddressRewriter::ForCountryCode(AddressCountryCode("ch"));
  EXPECT_EQ(ch.Rewrite(u"appenzell rhodes exterieures"),
            ch.Rewrite(u"appenzell ausserrhoden"));
  EXPECT_EQ(ch.Rewrite(u"prettigovia davos"), ch.Rewrite(u"prattigau davos"));
}

TEST(AddressRewriterTest, CL) {
  AddressRewriter cl =
      AddressRewriter::ForCountryCode(AddressCountryCode("cl"));
  EXPECT_EQ(cl.Rewrite(u"metropolitana de santiago de chile"),
            cl.Rewrite(u"metropolitana de santiago"));
}

TEST(AddressRewriterTest, CO) {
  AddressRewriter co =
      AddressRewriter::ForCountryCode(AddressCountryCode("co"));
  EXPECT_EQ(co.Rewrite(u"columbia"), co.Rewrite(u"colombia"));
}

TEST(AddressRewriterTest, DE) {
  AddressRewriter de =
      AddressRewriter::ForCountryCode(AddressCountryCode("de"));
  EXPECT_EQ(de.Rewrite(u"federal republic of germany"),
            de.Rewrite(u"deutschland"));
  EXPECT_EQ(de.Rewrite(u"germany"), de.Rewrite(u"bundesrepublik deutschland"));
}

TEST(AddressRewriterTest, DK) {
  AddressRewriter dk =
      AddressRewriter::ForCountryCode(AddressCountryCode("dk"));
  EXPECT_EQ(dk.Rewrite(u"denmark"), dk.Rewrite(u"danmark"));
}

TEST(AddressRewriterTest, ES) {
  AddressRewriter es =
      AddressRewriter::ForCountryCode(AddressCountryCode("es"));
  EXPECT_EQ(es.Rewrite(u"balearic islands"), es.Rewrite(u"islas baleares"));
}

TEST(AddressRewriterTest, FR) {
  AddressRewriter fr =
      AddressRewriter::ForCountryCode(AddressCountryCode("fr"));
  EXPECT_EQ(fr.Rewrite(u"couffouleux"), fr.Rewrite(u"coufouleux"));
}

TEST(AddressRewriterTest, GB) {
  AddressRewriter gb =
      AddressRewriter::ForCountryCode(AddressCountryCode("gb"));
  EXPECT_EQ(gb.Rewrite(u"north east lincolnshire"), gb.Rewrite(u"gb-nel"));

  EXPECT_NE(gb.Rewrite(u"norfolk"), gb.Rewrite(u"suffolk"));
}

TEST(AddressRewriterTest, GR) {
  AddressRewriter gr =
      AddressRewriter::ForCountryCode(AddressCountryCode("gr"));
  EXPECT_EQ(gr.Rewrite(u"aitolia kai akarnania"),
            gr.Rewrite(u"aitoloakarnania"));
}

TEST(AddressRewriterTest, HK) {
  AddressRewriter hk =
      AddressRewriter::ForCountryCode(AddressCountryCode("hk"));
  EXPECT_EQ(hk.Rewrite(u"hong kong"), hk.Rewrite(u"hk"));
}

TEST(AddressRewriterTest, ID) {
  AddressRewriter id =
      AddressRewriter::ForCountryCode(AddressCountryCode("id"));
  EXPECT_EQ(id.Rewrite(u"nanggroe aceh darussalam"), id.Rewrite(u"aceh"));
}

TEST(AddressRewriterTest, IE) {
  AddressRewriter ie =
      AddressRewriter::ForCountryCode(AddressCountryCode("ie"));
  EXPECT_EQ(ie.Rewrite(u"avenue"), ie.Rewrite(u"ave"));
}

TEST(AddressRewriterTest, IN) {
  AddressRewriter in =
      AddressRewriter::ForCountryCode(AddressCountryCode("in"));
  EXPECT_EQ(in.Rewrite(u"thiruvananthapuram"),
            in.Rewrite(u"tiruvananthapuram"));
  EXPECT_EQ(in.Rewrite(u"jammu & kashmir"), in.Rewrite(u"j&k"));
  EXPECT_EQ(in.Rewrite(u"cross-road"), in.Rewrite(u"xrd"));
  EXPECT_EQ(in.Rewrite(u"j & k"), in.Rewrite(u"j&k"));
  EXPECT_EQ(in.Rewrite(u"i.n.d.i.a"), in.Rewrite(u"india"));
  EXPECT_NE(in.Rewrite(u"i\\_n\\_d\\_i\\_a"), in.Rewrite(u"india"));
}

TEST(AddressRewriterTest, IT) {
  AddressRewriter it =
      AddressRewriter::ForCountryCode(AddressCountryCode("it"));
  EXPECT_EQ(it.Rewrite(u"trentino alto adige"),
            it.Rewrite(u"trentino sudtirol"));
}

TEST(AddressRewriterTest, LU) {
  AddressRewriter lu =
      AddressRewriter::ForCountryCode(AddressCountryCode("lu"));
  EXPECT_EQ(lu.Rewrite(u"esplanade"), lu.Rewrite(u"espl"));
}

TEST(AddressRewriterTest, MX) {
  AddressRewriter mx =
      AddressRewriter::ForCountryCode(AddressCountryCode("mx"));
  EXPECT_EQ(mx.Rewrite(u"estado de mexico"), mx.Rewrite(u"mexico"));
}

TEST(AddressRewriterTest, MY) {
  AddressRewriter my =
      AddressRewriter::ForCountryCode(AddressCountryCode("my"));
  EXPECT_EQ(my.Rewrite(u"malaysia"), my.Rewrite(u"my"));
}

TEST(AddressRewriterTest, NL) {
  AddressRewriter nl =
      AddressRewriter::ForCountryCode(AddressCountryCode("nl"));
  EXPECT_EQ(nl.Rewrite(u"nordholland"), nl.Rewrite(u"noord holland"));
}

TEST(AddressRewriterTest, NZ) {
  AddressRewriter nz =
      AddressRewriter::ForCountryCode(AddressCountryCode("nz"));
  EXPECT_EQ(nz.Rewrite(u"oceanbeach"), nz.Rewrite(u"ocean beach"));
}

TEST(AddressRewriterTest, PE) {
  AddressRewriter pe =
      AddressRewriter::ForCountryCode(AddressCountryCode("pe"));
  EXPECT_EQ(pe.Rewrite(u"avenida"), pe.Rewrite(u"av"));
}

TEST(AddressRewriterTest, PH) {
  AddressRewriter ph =
      AddressRewriter::ForCountryCode(AddressCountryCode("ph"));
  EXPECT_EQ(ph.Rewrite(u"philippines"), ph.Rewrite(u"ph"));
}

TEST(AddressRewriterTest, PL) {
  AddressRewriter pl =
      AddressRewriter::ForCountryCode(AddressCountryCode("pl"));
  EXPECT_EQ(pl.Rewrite(u"warmian masurian"), pl.Rewrite(u"warminsko"));
}

TEST(AddressRewriterTest, PR) {
  AddressRewriter pr =
      AddressRewriter::ForCountryCode(AddressCountryCode("pr"));
  EXPECT_EQ(pr.Rewrite(u"san juan antiguo"), pr.Rewrite(u"old san juan"));
}

TEST(AddressRewriterTest, PT) {
  AddressRewriter pt =
      AddressRewriter::ForCountryCode(AddressCountryCode("pt"));
  EXPECT_EQ(pt.Rewrite(u"doctor"), pt.Rewrite(u"doutor"));
}

TEST(AddressRewriterTest, RO) {
  AddressRewriter ro =
      AddressRewriter::ForCountryCode(AddressCountryCode("ro"));
  EXPECT_EQ(ro.Rewrite(u"romania"), ro.Rewrite(u"ro"));
}

TEST(AddressRewriterTest, RU) {
  AddressRewriter ru =
      AddressRewriter::ForCountryCode(AddressCountryCode("ru"));
  // TODO(rogerm): UTF8 matching isnt' working as expected. Fix it!
  EXPECT_NE(ru.Rewrite(u"россия"), ru.Rewrite(u"russia"));
}

TEST(AddressRewriterTest, SE) {
  AddressRewriter se =
      AddressRewriter::ForCountryCode(AddressCountryCode("se"));
  EXPECT_EQ(se.Rewrite(u"oestergoetland"), se.Rewrite(u"vastergoetland"));
}

TEST(AddressRewriterTest, TH) {
  AddressRewriter th =
      AddressRewriter::ForCountryCode(AddressCountryCode("th"));
  // TODO(rogerm): UTF8 matching isnt' working as expected. Fix it!
  EXPECT_NE(th.Rewrite(u"ประเทศไทย"), th.Rewrite(u"thailand"));
}

TEST(AddressRewriterTest, TR) {
  AddressRewriter tr =
      AddressRewriter::ForCountryCode(AddressCountryCode("tr"));
  EXPECT_EQ(tr.Rewrite(u"turkiye"), tr.Rewrite(u"turkey"));
}

TEST(AddressRewriterTest, US) {
  AddressRewriter us =
      AddressRewriter::ForCountryCode(AddressCountryCode("us"));
  EXPECT_EQ(us.Rewrite(u"ca"), us.Rewrite(u"california"));
  EXPECT_EQ(us.Rewrite(u"west virginia"), us.Rewrite(u"wv"));
  EXPECT_EQ(us.Rewrite(u"virginia"), us.Rewrite(u"va"));
  EXPECT_EQ(us.Rewrite(u"washington d c"), us.Rewrite(u"washington dc"));

  // Similar names, but not the same.
  EXPECT_NE(us.Rewrite(u"west virginia"), us.Rewrite(u"virginia"));
  EXPECT_NE(us.Rewrite(u"washington"), us.Rewrite(u"washington dc"));
}

TEST(AddressRewriterTest, VN) {
  AddressRewriter vn =
      AddressRewriter::ForCountryCode(AddressCountryCode("vn"));
  EXPECT_EQ(vn.Rewrite(u"viet nam"), vn.Rewrite(u"vietnam"));
}

TEST(AddressRewriterTest, ZA) {
  AddressRewriter za =
      AddressRewriter::ForCountryCode(AddressCountryCode("za"));
  EXPECT_EQ(za.Rewrite(u"republic of south africa"),
            za.Rewrite(u"south africa"));
}

}  // namespace
}  // namespace autofill

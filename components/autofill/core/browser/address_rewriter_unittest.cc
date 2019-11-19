// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_rewriter.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UTF8ToUTF16;
using autofill::AddressRewriter;

TEST(AddressRewriterTest, InvalidCountryCode) {
  AddressRewriter ad = AddressRewriter::ForCountryCode(UTF8ToUTF16("ZZZZ"));
  const base::string16 kSomeRandomText = UTF8ToUTF16("some random text");
  const base::string16 kOtherRandomText = UTF8ToUTF16("other random text");

  EXPECT_EQ(ad.Rewrite(kSomeRandomText), ad.Rewrite(kSomeRandomText));
  EXPECT_EQ(ad.Rewrite(kOtherRandomText), ad.Rewrite(kOtherRandomText));

  EXPECT_NE(ad.Rewrite(kSomeRandomText), ad.Rewrite(kOtherRandomText));
}

TEST(AddressRewriterTest, LastRule) {
  AddressRewriter last_rule = AddressRewriter::ForCustomRules("1\t2\n3\t4\n");
  AddressRewriter large_rewrite =
      AddressRewriter::ForCustomRules("1\tonelongrewrite\n2\tshort\n");

  EXPECT_EQ(last_rule.Rewrite(UTF8ToUTF16("3")),
            last_rule.Rewrite(UTF8ToUTF16("4")));
  // Checks if last rule works when previous rewrite is larger than last rule.
  EXPECT_EQ(large_rewrite.Rewrite(UTF8ToUTF16("2")),
            large_rewrite.Rewrite(UTF8ToUTF16("short")));
}

TEST(AddressRewriterTest, AD) {
  AddressRewriter ad = AddressRewriter::ForCountryCode(UTF8ToUTF16("ad"));
  EXPECT_EQ(ad.Rewrite(UTF8ToUTF16("parroquia de andorra la vella")),
            ad.Rewrite(UTF8ToUTF16("andorra la vella")));
  EXPECT_EQ(ad.Rewrite(UTF8ToUTF16("principal de andorra")),
            ad.Rewrite(UTF8ToUTF16("an")));
  EXPECT_EQ(ad.Rewrite(UTF8ToUTF16("or")), ad.Rewrite(UTF8ToUTF16("ordino")));
}

TEST(AddressRewriterTest, AR) {
  AddressRewriter ar = AddressRewriter::ForCountryCode(UTF8ToUTF16("ar"));
  EXPECT_EQ(ar.Rewrite(UTF8ToUTF16(
                "tierra del fuego antartida e islas del atlantico sur")),
            ar.Rewrite(UTF8ToUTF16("tierra del fuego")));
  EXPECT_EQ(ar.Rewrite(UTF8ToUTF16("ciudad autonoma de buenos aires")),
            ar.Rewrite(UTF8ToUTF16("capital federal")));
}

TEST(AddressRewriterTest, AU) {
  AddressRewriter au = AddressRewriter::ForCountryCode(UTF8ToUTF16("au"));
  EXPECT_EQ(au.Rewrite(UTF8ToUTF16("australian capital territory")),
            au.Rewrite(UTF8ToUTF16("act")));
  EXPECT_EQ(au.Rewrite(UTF8ToUTF16("jervis bay territory")),
            au.Rewrite(UTF8ToUTF16("jbt")));
}

TEST(AddressRewriterTest, BE) {
  AddressRewriter be = AddressRewriter::ForCountryCode(UTF8ToUTF16("be"));
  EXPECT_EQ(be.Rewrite(UTF8ToUTF16("brussels hoofdstedelijk gewest")),
            be.Rewrite(UTF8ToUTF16("region de bruxelles capitale")));
  EXPECT_EQ(be.Rewrite(UTF8ToUTF16("arrondissement administratif de foo")),
            be.Rewrite(UTF8ToUTF16("foo")));
}

TEST(AddressRewriterTest, BR) {
  AddressRewriter br = AddressRewriter::ForCountryCode(UTF8ToUTF16("br"));
  EXPECT_EQ(br.Rewrite(UTF8ToUTF16("rio grande do norte")),
            br.Rewrite(UTF8ToUTF16("rn")));
}

TEST(AddressRewriterTest, CA) {
  AddressRewriter ca = AddressRewriter::ForCountryCode(UTF8ToUTF16("ca"));
  EXPECT_EQ(ca.Rewrite(UTF8ToUTF16("qc")), ca.Rewrite(UTF8ToUTF16("quebec")));
  EXPECT_EQ(ca.Rewrite(UTF8ToUTF16("prince edward island")),
            ca.Rewrite(UTF8ToUTF16("pei")));
  EXPECT_EQ(ca.Rewrite(UTF8ToUTF16("prince edward island")),
            ca.Rewrite(UTF8ToUTF16("ile du prince edouard")));
  EXPECT_EQ(ca.Rewrite(UTF8ToUTF16("cul-de-sac")),
            ca.Rewrite(UTF8ToUTF16("cul de sac")));
  EXPECT_EQ(ca.Rewrite(UTF8ToUTF16("st")), ca.Rewrite(UTF8ToUTF16("street")));
  EXPECT_EQ(ca.Rewrite(UTF8ToUTF16("sainte")),
            ca.Rewrite(UTF8ToUTF16("saint")));
}

TEST(AddressRewriterTest, CH) {
  AddressRewriter ch = AddressRewriter::ForCountryCode(UTF8ToUTF16("ch"));
  EXPECT_EQ(ch.Rewrite(UTF8ToUTF16("appenzell rhodes exterieures")),
            ch.Rewrite(UTF8ToUTF16("appenzell ausserrhoden")));
  EXPECT_EQ(ch.Rewrite(UTF8ToUTF16("prettigovia davos")),
            ch.Rewrite(UTF8ToUTF16("prattigau davos")));
}

TEST(AddressRewriterTest, CL) {
  AddressRewriter cl = AddressRewriter::ForCountryCode(UTF8ToUTF16("cl"));
  EXPECT_EQ(
      cl.Rewrite(UTF8ToUTF16("aisen del general carlos ibanez del campo")),
      cl.Rewrite(UTF8ToUTF16("xi")));
  EXPECT_EQ(cl.Rewrite(UTF8ToUTF16("libertador general bernardo o'higgins")),
            cl.Rewrite(UTF8ToUTF16("vi")));
  EXPECT_EQ(cl.Rewrite(UTF8ToUTF16("metropolitana de santiago de chile")),
            cl.Rewrite(UTF8ToUTF16("metropolitana de santiago")));
}

TEST(AddressRewriterTest, CO) {
  AddressRewriter co = AddressRewriter::ForCountryCode(UTF8ToUTF16("co"));
  EXPECT_EQ(co.Rewrite(UTF8ToUTF16("columbia")),
            co.Rewrite(UTF8ToUTF16("colombia")));
}

TEST(AddressRewriterTest, DE) {
  AddressRewriter de = AddressRewriter::ForCountryCode(UTF8ToUTF16("de"));
  EXPECT_EQ(de.Rewrite(UTF8ToUTF16("federal republic of germany")),
            de.Rewrite(UTF8ToUTF16("deutschland")));
  EXPECT_EQ(de.Rewrite(UTF8ToUTF16("germany")),
            de.Rewrite(UTF8ToUTF16("bundesrepublik deutschland")));
}

TEST(AddressRewriterTest, DK) {
  AddressRewriter dk = AddressRewriter::ForCountryCode(UTF8ToUTF16("dk"));
  EXPECT_EQ(dk.Rewrite(UTF8ToUTF16("denmark")),
            dk.Rewrite(UTF8ToUTF16("danmark")));
}

TEST(AddressRewriterTest, ES) {
  AddressRewriter es = AddressRewriter::ForCountryCode(UTF8ToUTF16("es"));
  EXPECT_EQ(es.Rewrite(UTF8ToUTF16("balearic islands")),
            es.Rewrite(UTF8ToUTF16("islas baleares")));
}

TEST(AddressRewriterTest, FR) {
  AddressRewriter fr = AddressRewriter::ForCountryCode(UTF8ToUTF16("fr"));
  EXPECT_EQ(fr.Rewrite(UTF8ToUTF16("quatorzieme")),
            fr.Rewrite(UTF8ToUTF16("14")));
}

TEST(AddressRewriterTest, GB) {
  AddressRewriter gb = AddressRewriter::ForCountryCode(UTF8ToUTF16("gb"));
  EXPECT_EQ(gb.Rewrite(UTF8ToUTF16("north east lincolnshire")),
            gb.Rewrite(UTF8ToUTF16("gb-nel")));

  EXPECT_NE(gb.Rewrite(UTF8ToUTF16("norfolk")),
            gb.Rewrite(UTF8ToUTF16("suffolk")));
}

TEST(AddressRewriterTest, GR) {
  AddressRewriter gr = AddressRewriter::ForCountryCode(UTF8ToUTF16("gr"));
  EXPECT_EQ(gr.Rewrite(UTF8ToUTF16("aitolia kai akarnania")),
            gr.Rewrite(UTF8ToUTF16("aitoloakarnania")));
}

TEST(AddressRewriterTest, HK) {
  AddressRewriter hk = AddressRewriter::ForCountryCode(UTF8ToUTF16("hk"));
  EXPECT_EQ(hk.Rewrite(UTF8ToUTF16("hong kong")),
            hk.Rewrite(UTF8ToUTF16("hk")));
}

TEST(AddressRewriterTest, ID) {
  AddressRewriter id = AddressRewriter::ForCountryCode(UTF8ToUTF16("id"));
  EXPECT_EQ(id.Rewrite(UTF8ToUTF16("nanggroe aceh darussalam")),
            id.Rewrite(UTF8ToUTF16("aceh")));
}

TEST(AddressRewriterTest, IE) {
  AddressRewriter ie = AddressRewriter::ForCountryCode(UTF8ToUTF16("ie"));
  EXPECT_EQ(ie.Rewrite(UTF8ToUTF16("avenue")), ie.Rewrite(UTF8ToUTF16("ave")));
}

TEST(AddressRewriterTest, IN) {
  AddressRewriter in = AddressRewriter::ForCountryCode(UTF8ToUTF16("in"));
  EXPECT_EQ(in.Rewrite(UTF8ToUTF16("thiruvananthapuram")),
            in.Rewrite(UTF8ToUTF16("tiruvananthapuram")));
  EXPECT_EQ(in.Rewrite(UTF8ToUTF16("jammu & kashmir")),
            in.Rewrite(UTF8ToUTF16("j&k")));
  EXPECT_EQ(in.Rewrite(UTF8ToUTF16("cross-road")),
            in.Rewrite(UTF8ToUTF16("xrd")));
  EXPECT_EQ(in.Rewrite(UTF8ToUTF16("j & k")), in.Rewrite(UTF8ToUTF16("j&k")));
  EXPECT_EQ(in.Rewrite(UTF8ToUTF16("i.n.d.i.a")),
            in.Rewrite(UTF8ToUTF16("india")));
  EXPECT_NE(in.Rewrite(UTF8ToUTF16("i\\_n\\_d\\_i\\_a")),
            in.Rewrite(UTF8ToUTF16("india")));
}

TEST(AddressRewriterTest, IT) {
  AddressRewriter it = AddressRewriter::ForCountryCode(UTF8ToUTF16("it"));
  EXPECT_EQ(it.Rewrite(UTF8ToUTF16("trentino alto adige")),
            it.Rewrite(UTF8ToUTF16("trentino sudtirol")));
}

TEST(AddressRewriterTest, LU) {
  AddressRewriter lu = AddressRewriter::ForCountryCode(UTF8ToUTF16("lu"));
  EXPECT_EQ(lu.Rewrite(UTF8ToUTF16("esplanade")),
            lu.Rewrite(UTF8ToUTF16("espl")));
}

TEST(AddressRewriterTest, MX) {
  AddressRewriter mx = AddressRewriter::ForCountryCode(UTF8ToUTF16("mx"));
  EXPECT_EQ(mx.Rewrite(UTF8ToUTF16("estado de mexico")),
            mx.Rewrite(UTF8ToUTF16("mexico")));
}

TEST(AddressRewriterTest, MY) {
  AddressRewriter my = AddressRewriter::ForCountryCode(UTF8ToUTF16("my"));
  EXPECT_EQ(my.Rewrite(UTF8ToUTF16("malaysia")), my.Rewrite(UTF8ToUTF16("my")));
}

TEST(AddressRewriterTest, NL) {
  AddressRewriter nl = AddressRewriter::ForCountryCode(UTF8ToUTF16("nl"));
  EXPECT_EQ(nl.Rewrite(UTF8ToUTF16("nordholland")),
            nl.Rewrite(UTF8ToUTF16("noord holland")));
}

TEST(AddressRewriterTest, NZ) {
  AddressRewriter nz = AddressRewriter::ForCountryCode(UTF8ToUTF16("nz"));
  EXPECT_EQ(nz.Rewrite(UTF8ToUTF16("oceanbeach")),
            nz.Rewrite(UTF8ToUTF16("ocean beach")));
}

TEST(AddressRewriterTest, PE) {
  AddressRewriter pe = AddressRewriter::ForCountryCode(UTF8ToUTF16("pe"));
  EXPECT_EQ(pe.Rewrite(UTF8ToUTF16("avenida")), pe.Rewrite(UTF8ToUTF16("av")));
}

TEST(AddressRewriterTest, PH) {
  AddressRewriter ph = AddressRewriter::ForCountryCode(UTF8ToUTF16("ph"));
  EXPECT_EQ(ph.Rewrite(UTF8ToUTF16("philippines")),
            ph.Rewrite(UTF8ToUTF16("ph")));
}

TEST(AddressRewriterTest, PL) {
  AddressRewriter pl = AddressRewriter::ForCountryCode(UTF8ToUTF16("pl"));
  EXPECT_EQ(pl.Rewrite(UTF8ToUTF16("warmian masurian")),
            pl.Rewrite(UTF8ToUTF16("warminsko")));
}

TEST(AddressRewriterTest, PR) {
  AddressRewriter pr = AddressRewriter::ForCountryCode(UTF8ToUTF16("pr"));
  EXPECT_EQ(pr.Rewrite(UTF8ToUTF16("san juan antiguo")),
            pr.Rewrite(UTF8ToUTF16("old san juan")));
}

TEST(AddressRewriterTest, PT) {
  AddressRewriter pt = AddressRewriter::ForCountryCode(UTF8ToUTF16("pt"));
  EXPECT_EQ(pt.Rewrite(UTF8ToUTF16("doctor")),
            pt.Rewrite(UTF8ToUTF16("doutor")));
}

TEST(AddressRewriterTest, RO) {
  AddressRewriter ro = AddressRewriter::ForCountryCode(UTF8ToUTF16("ro"));
  EXPECT_EQ(ro.Rewrite(UTF8ToUTF16("romania")), ro.Rewrite(UTF8ToUTF16("ro")));
}

TEST(AddressRewriterTest, RU) {
  AddressRewriter ru = AddressRewriter::ForCountryCode(UTF8ToUTF16("ru"));
  // TODO(rogerm): UTF8 matching isnt' working as expected. Fix it!
  EXPECT_NE(ru.Rewrite(UTF8ToUTF16("россия")),
            ru.Rewrite(UTF8ToUTF16("russia")));
}

TEST(AddressRewriterTest, SE) {
  AddressRewriter se = AddressRewriter::ForCountryCode(UTF8ToUTF16("se"));
  EXPECT_EQ(se.Rewrite(UTF8ToUTF16("oestergoetland")),
            se.Rewrite(UTF8ToUTF16("vastergoetland")));
}

TEST(AddressRewriterTest, TH) {
  AddressRewriter th = AddressRewriter::ForCountryCode(UTF8ToUTF16("th"));
  // TODO(rogerm): UTF8 matching isnt' working as expected. Fix it!
  EXPECT_NE(th.Rewrite(UTF8ToUTF16("ประเทศไทย")),
            th.Rewrite(UTF8ToUTF16("thailand")));
}

TEST(AddressRewriterTest, TR) {
  AddressRewriter tr = AddressRewriter::ForCountryCode(UTF8ToUTF16("tr"));
  EXPECT_EQ(tr.Rewrite(UTF8ToUTF16("turkiye")),
            tr.Rewrite(UTF8ToUTF16("turkey")));
}

TEST(AddressRewriterTest, US) {
  AddressRewriter us = AddressRewriter::ForCountryCode(UTF8ToUTF16("us"));
  EXPECT_EQ(us.Rewrite(UTF8ToUTF16("ca")),
            us.Rewrite(UTF8ToUTF16("california")));
  EXPECT_EQ(us.Rewrite(UTF8ToUTF16("west virginia")),
            us.Rewrite(UTF8ToUTF16("wv")));
  EXPECT_EQ(us.Rewrite(UTF8ToUTF16("virginia")), us.Rewrite(UTF8ToUTF16("va")));
  EXPECT_EQ(us.Rewrite(UTF8ToUTF16("washington d c")),
            us.Rewrite(UTF8ToUTF16("washington dc")));

  // Similar names, but not the same.
  EXPECT_NE(us.Rewrite(UTF8ToUTF16("west virginia")),
            us.Rewrite(UTF8ToUTF16("virginia")));
  EXPECT_NE(us.Rewrite(UTF8ToUTF16("washington")),
            us.Rewrite(UTF8ToUTF16("washington dc")));
}

TEST(AddressRewriterTest, VN) {
  AddressRewriter vn = AddressRewriter::ForCountryCode(UTF8ToUTF16("vn"));
  EXPECT_EQ(vn.Rewrite(UTF8ToUTF16("viet nam")),
            vn.Rewrite(UTF8ToUTF16("vietnam")));
}

TEST(AddressRewriterTest, ZA) {
  AddressRewriter za = AddressRewriter::ForCountryCode(UTF8ToUTF16("za"));
  EXPECT_EQ(za.Rewrite(UTF8ToUTF16("republic of south africa")),
            za.Rewrite(UTF8ToUTF16("south africa")));
}

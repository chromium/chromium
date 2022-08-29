// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/diacritic_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash::string_matching {
class DiacriticUtilsTest : public testing::Test {};

TEST(DiacriticUtilsTest, TestEqual) {
  DiacriticUtils util;
  EXPECT_EQ(util.RemoveDiacritics(u"français"), u"francais");
  EXPECT_EQ(util.RemoveDiacritics(u"déjà"), u"deja");
  EXPECT_EQ(util.RemoveDiacritics(u"Español"), u"Espanol");
  EXPECT_EQ(util.RemoveDiacritics(u"École"), u"Ecole");
  EXPECT_EQ(util.RemoveDiacritics(u"cœur"), u"coeur");
  EXPECT_EQ(util.RemoveDiacritics(u"København"), u"Kobenhavn");
  EXPECT_EQ(util.RemoveDiacritics(u"ångström"), u"angstrom");
  EXPECT_EQ(util.RemoveDiacritics(u"Neuchâtel"), u"Neuchatel");
  EXPECT_EQ(util.RemoveDiacritics(u"jamón"), u"jamon");
  EXPECT_EQ(util.RemoveDiacritics(u"NOËL"), u"NOEL");
}

TEST(DiacriticUtilsTest, TestNotEqual) {
  DiacriticUtils util;
  EXPECT_NE(util.RemoveDiacritics(u"français"), u"français");
  EXPECT_NE(util.RemoveDiacritics(u"Déjà"), u"deja");
  EXPECT_NE(util.RemoveDiacritics(u"español"), u"français");
  EXPECT_NE(util.RemoveDiacritics(u"École"), u"ecole");
  EXPECT_NE(util.RemoveDiacritics(u"œufs"), u"coeur");
  EXPECT_NE(util.RemoveDiacritics(u"København"), u"Copenhagen");
  EXPECT_NE(util.RemoveDiacritics(u"ångström"), u"angstroms");
  EXPECT_NE(util.RemoveDiacritics(u"Neuchâtel"), u"Newcastle");
  EXPECT_NE(util.RemoveDiacritics(u"jamón"), u"jambon");
  EXPECT_NE(util.RemoveDiacritics(u"NOËL"), u"Christmas");
}
}  // namespace ash::string_matching

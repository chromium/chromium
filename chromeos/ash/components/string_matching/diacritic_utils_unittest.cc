// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/diacritic_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash::string_matching {
class DiacriticUtilsTest : public testing::Test {};

TEST(DiacriticUtilsTest, TestEqual) {
  EXPECT_EQ(RemoveDiacritics(u"français"), u"francais");
  EXPECT_EQ(RemoveDiacritics(u"déjà"), u"deja");
  EXPECT_EQ(RemoveDiacritics(u"Español"), u"Espanol");
  EXPECT_EQ(RemoveDiacritics(u"École"), u"Ecole");
  EXPECT_EQ(RemoveDiacritics(u"còēur"), u"coeur");
  EXPECT_EQ(RemoveDiacritics(u"København"), u"Kobenhavn");
  EXPECT_EQ(RemoveDiacritics(u"ångström"), u"angstrom");
  EXPECT_EQ(RemoveDiacritics(u"Neuchâtel"), u"Neuchatel");
  EXPECT_EQ(RemoveDiacritics(u"jamón"), u"jamon");
  EXPECT_EQ(RemoveDiacritics(u"NOËL"), u"NOEL");
}

TEST(DiacriticUtilsTest, TestNotEqual) {
  EXPECT_NE(RemoveDiacritics(u"français"), u"français");
  EXPECT_NE(RemoveDiacritics(u"Déjà"), u"deja");
  EXPECT_NE(RemoveDiacritics(u"español"), u"français");
  EXPECT_NE(RemoveDiacritics(u"École"), u"ecole");
  EXPECT_NE(RemoveDiacritics(u"çufs"), u"coeur");
  EXPECT_NE(RemoveDiacritics(u"København"), u"Copenhagen");
  EXPECT_NE(RemoveDiacritics(u"ångström"), u"angstroms");
  EXPECT_NE(RemoveDiacritics(u"Neuchâtel"), u"Newcastle");
  EXPECT_NE(RemoveDiacritics(u"jamón"), u"jambon");
  EXPECT_NE(RemoveDiacritics(u"NOËL"), u"Christmas");
}
}  // namespace ash::string_matching

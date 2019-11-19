// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_util.h"

#include "base/command_line.h"
#include "components/translate/core/common/translate_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

typedef testing::Test TranslateUtilTest;

TEST_F(TranslateUtilTest, SecurityOrigin) {
  GURL origin = translate::GetTranslateSecurityOrigin();
  EXPECT_EQ(std::string(translate::kSecurityOrigin), origin.spec());

  const std::string running_origin("http://www.tamurayukari.com/");
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(translate::switches::kTranslateSecurityOrigin,
                                  running_origin);
  GURL modified_origin = translate::GetTranslateSecurityOrigin();
  EXPECT_EQ(running_origin, modified_origin.spec());
}


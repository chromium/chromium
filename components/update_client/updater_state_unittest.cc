// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/updater_state.h"

#include "base/macros.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

class UpdaterStateTest : public testing::Test {
 public:
  UpdaterStateTest() {}
  ~UpdaterStateTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdaterStateTest);
};

TEST_F(UpdaterStateTest, Serialize) {
  UpdaterState updater_state(false);

  updater_state.updater_name_ = "the updater";
  updater_state.updater_version_ = base::Version("1.0");
  updater_state.last_autoupdate_started_ = base::Time::NowFromSystemTime();
  updater_state.last_checked_ = base::Time::NowFromSystemTime();
  updater_state.is_enterprise_managed_ = false;
  updater_state.is_autoupdate_check_enabled_ = true;
  updater_state.update_policy_ = 1;

  auto attributes = updater_state.BuildAttributes();

  // Sanity check all members.
  EXPECT_STREQ("the updater", attributes.at("name").c_str());
  EXPECT_STREQ("1.0", attributes.at("version").c_str());
  EXPECT_STREQ("0", attributes.at("laststarted").c_str());
  EXPECT_STREQ("0", attributes.at("lastchecked").c_str());
  EXPECT_STREQ("0", attributes.at("domainjoined").c_str());
  EXPECT_STREQ("1", attributes.at("autoupdatecheckenabled").c_str());
  EXPECT_STREQ("1", attributes.at("updatepolicy").c_str());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if defined(OS_WIN)
  // The value of "ismachine".
  EXPECT_STREQ("0", UpdaterState::GetState(false)->at("ismachine").c_str());
  EXPECT_STREQ("1", UpdaterState::GetState(true)->at("ismachine").c_str());

  // The name of the Windows updater for Chrome.
  EXPECT_STREQ("Omaha", UpdaterState::GetState(false)->at("name").c_str());
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  // MacOS does not serialize "ismachine".
  EXPECT_EQ(0UL, UpdaterState::GetState(false)->count("ismachine"));
  EXPECT_EQ(0UL, UpdaterState::GetState(true)->count("ismachine"));
  EXPECT_STREQ("Keystone", UpdaterState::GetState(false)->at("name").c_str());
#endif  // OS_WIN
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Tests some of the remaining values.
  updater_state = UpdaterState(false);

  // Don't serialize an invalid version if it could not be read.
  updater_state.updater_version_ = base::Version();
  attributes = updater_state.BuildAttributes();
  EXPECT_EQ(0u, attributes.count("version"));

  updater_state.updater_version_ = base::Version("0.0.0.0");
  attributes = updater_state.BuildAttributes();
  EXPECT_STREQ("0.0.0.0", attributes.at("version").c_str());

  updater_state.last_autoupdate_started_ =
      base::Time::NowFromSystemTime() - base::TimeDelta::FromDays(15);
  attributes = updater_state.BuildAttributes();
  EXPECT_STREQ("408", attributes.at("laststarted").c_str());

  updater_state.last_autoupdate_started_ =
      base::Time::NowFromSystemTime() - base::TimeDelta::FromDays(90);
  attributes = updater_state.BuildAttributes();
  EXPECT_STREQ("1344", attributes.at("laststarted").c_str());

  // Don't serialize the time if it could not be read.
  updater_state.last_autoupdate_started_ = base::Time();
  attributes = updater_state.BuildAttributes();
  EXPECT_EQ(0u, attributes.count("laststarted"));

  updater_state.last_checked_ =
      base::Time::NowFromSystemTime() - base::TimeDelta::FromDays(15);
  attributes = updater_state.BuildAttributes();
  EXPECT_STREQ("408", attributes.at("lastchecked").c_str());

  updater_state.last_checked_ =
      base::Time::NowFromSystemTime() - base::TimeDelta::FromDays(90);
  attributes = updater_state.BuildAttributes();
  EXPECT_STREQ("1344", attributes.at("lastchecked").c_str());

  // Don't serialize the time if it could not be read (the value is invalid).
  updater_state.last_checked_ = base::Time();
  attributes = updater_state.BuildAttributes();
  EXPECT_EQ(0u, attributes.count("lastchecked"));

  updater_state.is_enterprise_managed_ = true;
  attributes = updater_state.BuildAttributes();
  EXPECT_STREQ("1", attributes.at("domainjoined").c_str());

  updater_state.is_autoupdate_check_enabled_ = false;
  attributes = updater_state.BuildAttributes();
  EXPECT_STREQ("0", attributes.at("autoupdatecheckenabled").c_str());

  updater_state.update_policy_ = 0;
  attributes = updater_state.BuildAttributes();
  EXPECT_STREQ("0", attributes.at("updatepolicy").c_str());

  updater_state.update_policy_ = -1;
  attributes = updater_state.BuildAttributes();
  EXPECT_STREQ("-1", attributes.at("updatepolicy").c_str());
}

}  // namespace update_client

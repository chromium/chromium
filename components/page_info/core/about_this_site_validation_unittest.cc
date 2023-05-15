// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/about_this_site_validation.h"

#include "base/test/scoped_feature_list.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_info::about_this_site_validation {

proto::Hyperlink GetSampleSource() {
  proto::Hyperlink link;
  link.set_label("Example Source");
  link.set_url("https://example.com");
  return link;
}

proto::SiteDescription GetSampleDescription() {
  proto::SiteDescription description;
  description.set_name("Example");
  description.set_description("Example Entity");
  *description.mutable_source() = GetSampleSource();
  description.set_lang("en_US");
  return description;
}

proto::SiteFirstSeen GetSampleFirstSeen() {
  proto::SiteFirstSeen first_seen;
  first_seen.set_count(5);
  first_seen.set_unit(proto::UNIT_DAYS);
  first_seen.set_precision(proto::PRECISION_ABOUT);
  return first_seen;
}

proto::MoreAbout GetSampleMoreAbout() {
  proto::MoreAbout more_about;
  more_about.set_url("https://example.com");
  return more_about;
}

proto::AboutThisSiteMetadata GetSampleMetaData() {
  proto::AboutThisSiteMetadata metadata;
  auto* site_info = metadata.mutable_site_info();
  *site_info->mutable_description() = GetSampleDescription();
  *site_info->mutable_first_seen() = GetSampleFirstSeen();
  *site_info->mutable_more_about() = GetSampleMoreAbout();
  return metadata;
}

// Tests that correct proto messages are accepted.
TEST(AboutThisSiteValidation, ValidateProtos) {
  auto metadata = GetSampleMetaData();
  EXPECT_EQ(ValidateMetadata(metadata), AboutThisSiteStatus::kValid);

  // The proto should still be valid without a timestamp.
  metadata.mutable_site_info()->clear_first_seen();
  EXPECT_EQ(ValidateMetadata(metadata), AboutThisSiteStatus::kValid);
}

TEST(AboutThisSiteValidation, InvalidSiteInfoProto) {
  proto::AboutThisSiteMetadata metadata;
  EXPECT_EQ(ValidateMetadata(metadata), AboutThisSiteStatus::kMissingSiteInfo);
  metadata.mutable_site_info();
  EXPECT_EQ(ValidateMetadata(metadata), AboutThisSiteStatus::kEmptySiteInfo);
}

TEST(AboutThisSiteValidation, InvalidDescription) {
  proto::SiteDescription description = GetSampleDescription();
  description.clear_description();

  EXPECT_EQ(ValidateDescription(description),
            AboutThisSiteStatus::kMissingDescriptionDescription);

  description = GetSampleDescription();
  description.clear_name();
  EXPECT_EQ(ValidateDescription(description),
            AboutThisSiteStatus::kMissingDescriptionName);

  description = GetSampleDescription();
  description.clear_lang();
  EXPECT_EQ(ValidateDescription(description),
            AboutThisSiteStatus::kMissingDescriptionLang);

  description = GetSampleDescription();
  description.clear_source();
  EXPECT_EQ(ValidateDescription(description),
            AboutThisSiteStatus::kMissingDescriptionSource);
}

TEST(AboutThisSiteValidation, OnlyMoreAbout) {
  proto::SiteInfo site_info;
  *site_info.mutable_more_about() = GetSampleMoreAbout();

  EXPECT_EQ(ValidateSiteInfo(site_info), AboutThisSiteStatus::kValid);
}

TEST(AboutThisSiteValidation, InvalidSource) {
  proto::Hyperlink source = GetSampleSource();
  source.clear_label();
  EXPECT_EQ(ValidateSource(source), AboutThisSiteStatus::kIncompleteSource);

  source = GetSampleSource();
  source.clear_url();
  EXPECT_EQ(ValidateSource(source), AboutThisSiteStatus::kIncompleteSource);

  source = GetSampleSource();
  source.set_url("example.com");
  EXPECT_EQ(ValidateSource(source), AboutThisSiteStatus::kInvalidSource);

  source.set_url("ftp://example.com");
  EXPECT_EQ(ValidateSource(source), AboutThisSiteStatus::kInvalidSource);
}

TEST(AboutThisSiteValidation, InvalidFirstSeenDuration) {
  proto::SiteFirstSeen first_seen = GetSampleFirstSeen();
  first_seen.clear_count();
  EXPECT_EQ(ValidateFirstSeen(first_seen),
            AboutThisSiteStatus::kIncompleteTimeStamp);

  first_seen = GetSampleFirstSeen();
  first_seen.set_unit(proto::UNIT_UNSPECIFIED);
  EXPECT_EQ(ValidateFirstSeen(first_seen),
            AboutThisSiteStatus::kInvalidTimeStamp);
}

TEST(AboutThisSiteValidation, InvalidMoreAbout) {
  proto::MoreAbout more_about = GetSampleMoreAbout();
  more_about.clear_url();
  EXPECT_EQ(ValidateMoreAbout(more_about),
            AboutThisSiteStatus::kInvalidMoreAbout);

  more_about = GetSampleMoreAbout();
  more_about.set_url("not a url");
  EXPECT_EQ(ValidateMoreAbout(more_about),
            AboutThisSiteStatus::kInvalidMoreAbout);
}

TEST(AboutThisSiteValidation, MissingMoreAbout) {
  proto::AboutThisSiteMetadata meta_data = GetSampleMetaData();
  EXPECT_EQ(ValidateMetadata(meta_data), AboutThisSiteStatus::kValid);

  meta_data.mutable_site_info()->clear_more_about();
  EXPECT_EQ(ValidateMetadata(meta_data),
            AboutThisSiteStatus::kMissingMoreAbout);
}

}  // namespace page_info::about_this_site_validation

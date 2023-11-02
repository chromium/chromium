// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_settings.h"
#include <string>
#include "base/time/time.h"
#include "components/autofill_assistant/browser/mock_client.h"

namespace autofill_assistant {

namespace {

using testing::Field;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

class ClientSettingsTest : public testing::Test {
 protected:
  ClientSettingsTest() {}
  ~ClientSettingsTest() override {}

  void AddDisplayStringToProto(ClientSettingsProto::DisplayStringId id,
                               const std::string str,
                               ClientSettingsProto& proto) {
    ClientSettingsProto::DisplayString* display_str =
        proto.add_display_strings();
    display_str->set_id(id);
    display_str->set_value(str);
  }
};

TEST_F(ClientSettingsTest, CheckLegacyOverlayImage) {
  ClientSettingsProto proto;
  proto.mutable_overlay_image()->set_image_url(
      "https://www.example.com/favicon.ico");
  proto.mutable_overlay_image()->mutable_image_size()->set_dp(32);

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ASSERT_TRUE(settings.overlay_image.has_value());
  EXPECT_EQ(settings.overlay_image->image_drawable().bitmap().url(),
            "https://www.example.com/favicon.ico");
}

TEST_F(ClientSettingsTest, NoImageSizeInvalidOverlay) {
  ClientSettingsProto proto;
  proto.mutable_overlay_image()->set_image_url(
      "https://www.example.com/favicon.ico");

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_FALSE(settings.overlay_image.has_value());
}

TEST_F(ClientSettingsTest, TextWithoutColorOrSizeInvalidOverlay) {
  ClientSettingsProto proto;
  proto.mutable_overlay_image()->set_image_url(
      "https://www.example.com/favicon.ico");
  proto.mutable_overlay_image()->set_text("Test text");

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_FALSE(settings.overlay_image.has_value());
}

TEST_F(ClientSettingsTest, NoDisplayStringsNoLocale) {
  ClientSettingsProto proto;
  ClientSettings settings;
  settings.UpdateFromProto(proto);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, IsEmpty()),
                    Field(&ClientSettings::display_strings, IsEmpty())));
}

TEST_F(ClientSettingsTest, DisplayStringsSetWithValidLocale) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, "en-US"),
                    Field(&ClientSettings::display_strings,
                          UnorderedElementsAre(
                              Pair(ClientSettingsProto::GIVE_UP, "give_up"),
                              Pair(ClientSettingsProto::MAYBE_GIVE_UP,
                                   "maybe_give_up")))));
}

TEST_F(ClientSettingsTest, DisplayStringsDoesNotMergeWhenLocaleEmpty) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  AddDisplayStringToProto(ClientSettingsProto::DEFAULT_ERROR, "default_error",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "new_give_up",
                          proto_new);
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(
      settings,
      AllOf(Field(&ClientSettings::display_strings_locale, "en-US"),
            Field(&ClientSettings::display_strings,
                  UnorderedElementsAre(
                      Pair(ClientSettingsProto::GIVE_UP, "give_up"),
                      Pair(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up"),
                      Pair(ClientSettingsProto::DEFAULT_ERROR,
                           "default_error")))));
}

TEST_F(ClientSettingsTest, DisplayStringsMergedWithSameLocale) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  AddDisplayStringToProto(ClientSettingsProto::DEFAULT_ERROR, "default_error",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "new_give_up",
                          proto_new);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP,
                          "new_maybe_give_up", proto_new);
  proto_new.set_display_strings_locale("en-US");
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(
      settings,
      AllOf(
          Field(&ClientSettings::display_strings_locale, "en-US"),
          Field(
              &ClientSettings::display_strings,
              UnorderedElementsAre(
                  Pair(ClientSettingsProto::GIVE_UP, "new_give_up"),
                  Pair(ClientSettingsProto::MAYBE_GIVE_UP, "new_maybe_give_up"),
                  Pair(ClientSettingsProto::DEFAULT_ERROR, "default_error")))));
}

TEST_F(ClientSettingsTest,
       DisplayStringsClearedForLocaleSwitchWithEmptyStrings) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  proto_new.set_display_strings_locale("fr-FR");
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, "fr-FR"),
                    Field(&ClientSettings::display_strings, IsEmpty())));
}

TEST_F(ClientSettingsTest, DisplayStringsReplacedForLocaleSwitch) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  proto_new.set_display_strings_locale("fr-FR");
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "fr_give_up",
                          proto_new);
  AddDisplayStringToProto(ClientSettingsProto::DEFAULT_ERROR,
                          "fr_default_error", proto_new);
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, "fr-FR"),
                    Field(&ClientSettings::display_strings,
                          UnorderedElementsAre(
                              Pair(ClientSettingsProto::GIVE_UP, "fr_give_up"),
                              Pair(ClientSettingsProto::DEFAULT_ERROR,
                                   "fr_default_error")))));
}

TEST_F(ClientSettingsTest, EmptyUpdateDoesNotResetDisplayStrings) {
  ClientSettingsProto proto;
  AddDisplayStringToProto(ClientSettingsProto::GIVE_UP, "give_up", proto);
  AddDisplayStringToProto(ClientSettingsProto::MAYBE_GIVE_UP, "maybe_give_up",
                          proto);
  proto.set_display_strings_locale("en-US");
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ClientSettingsProto proto_new;
  settings.UpdateFromProto(proto_new);
  EXPECT_THAT(settings,
              AllOf(Field(&ClientSettings::display_strings_locale, "en-US"),
                    Field(&ClientSettings::display_strings,
                          UnorderedElementsAre(
                              Pair(ClientSettingsProto::GIVE_UP, "give_up"),
                              Pair(ClientSettingsProto::MAYBE_GIVE_UP,
                                   "maybe_give_up")))));
}

TEST_F(ClientSettingsTest, PeriodicIntervalValues) {
  ClientSettingsProto proto;
  proto.set_periodic_script_check_interval_ms(987);
  proto.set_periodic_element_check_interval_ms(876);
  proto.set_element_position_update_interval_ms(765);
  proto.set_short_wait_for_element_deadline_ms(654);

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_EQ(settings.periodic_script_check_interval, base::Milliseconds(987));
  EXPECT_EQ(settings.periodic_element_check_interval, base::Milliseconds(876));
  EXPECT_EQ(settings.element_position_update_interval, base::Milliseconds(765));
  EXPECT_EQ(settings.short_wait_for_element_deadline, base::Milliseconds(654));
}

TEST_F(ClientSettingsTest, BoxModel) {
  ClientSettingsProto proto;
  proto.set_box_model_check_interval_ms(543);
  proto.set_box_model_check_count(6);

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_EQ(settings.box_model_check_interval, base::Milliseconds(543));
  EXPECT_EQ(settings.box_model_check_count, 6);
}

TEST_F(ClientSettingsTest, DocumentReadyCheckTimeoutValue) {
  ClientSettingsProto proto;
  proto.set_document_ready_check_timeout_ms(432);
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_EQ(settings.document_ready_check_timeout, base::Milliseconds(432));
}

TEST_F(ClientSettingsTest, CancelDelayValue) {
  ClientSettingsProto proto;
  proto.set_cancel_delay_ms(321);

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_EQ(settings.cancel_delay, base::Milliseconds(321));
}

TEST_F(ClientSettingsTest, Tap) {
  ClientSettingsProto proto;
  proto.set_tap_count(8);
  proto.set_tap_tracking_duration_ms(210);
  proto.set_tap_shutdown_delay_ms(109);

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_EQ(settings.tap_count, 8);
  EXPECT_EQ(settings.tap_tracking_duration, base::Milliseconds(210));
  EXPECT_EQ(settings.tap_shutdown_delay, base::Milliseconds(109));
}

TEST_F(ClientSettingsTest, TalkBackSheetSizeFractionValue) {
  ClientSettingsProto proto;
  proto.set_talkback_sheet_size_fraction(.6f);

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_EQ(settings.talkback_sheet_size_fraction, .6f);
}

TEST_F(ClientSettingsTest, SelectorObserver) {
  ClientSettingsProto proto;
  proto.set_selector_observer_debounce_interval_ms(987);
  proto.set_selector_observer_extra_timeout_ms(876);

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_EQ(settings.selector_observer_extra_timeout, base::Milliseconds(876));
  EXPECT_EQ(settings.selector_observer_debounce_interval,
            base::Milliseconds(987));
}

TEST_F(ClientSettingsTest, SlowWarningSettings) {
  ClientSettingsProto proto;
  ClientSettingsProto::SlowWarningSettings slow_settings;

  slow_settings.set_enable_slow_connection_warnings(true);
  slow_settings.set_enable_slow_website_warnings(true);
  slow_settings.set_only_show_warning_once(true);
  slow_settings.set_only_show_connection_warning_once(true);
  slow_settings.set_only_show_website_warning_once(true);
  slow_settings.set_warning_delay_ms(987);
  slow_settings.set_slow_roundtrip_threshold_ms(876);
  slow_settings.set_max_consecutive_slow_roundtrips(765);
  slow_settings.set_minimum_warning_message_duration_ms(654);
  slow_settings.set_message_mode(
      ClientSettingsProto::SlowWarningSettings::CONCATENATE);
  slow_settings.set_slow_connection_message("Slow Connection");
  slow_settings.set_slow_website_message("Slow Website");

  *proto.mutable_slow_warning_settings() = slow_settings;
  ClientSettings settings;
  settings.UpdateFromProto(proto);

  EXPECT_TRUE(settings.enable_slow_connection_warnings);
  EXPECT_TRUE(settings.enable_slow_website_warnings);
  EXPECT_TRUE(settings.only_show_warning_once);
  EXPECT_TRUE(settings.only_show_connection_warning_once);
  EXPECT_TRUE(settings.only_show_website_warning_once);
  EXPECT_EQ(settings.warning_delay, base::Milliseconds(987));
  EXPECT_EQ(settings.slow_roundtrip_threshold, base::Milliseconds(876));
  EXPECT_EQ(settings.max_consecutive_slow_roundtrips, 765);
  EXPECT_EQ(settings.minimum_warning_duration, base::Milliseconds(654));
  EXPECT_EQ(settings.message_mode,
            ClientSettingsProto::SlowWarningSettings::CONCATENATE);
  EXPECT_EQ(settings.slow_connection_message, "Slow Connection");
  EXPECT_EQ(settings.slow_website_message, "Slow Website");
}
}  // namespace
}  // namespace autofill_assistant

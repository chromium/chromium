// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_store.h"

#include <unordered_set>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/notes/core/templates/note_template.h"
#include "components/content_creation/notes/core/templates/template_storage.pb.h"
#include "components/content_creation/notes/core/templates/template_types.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_creation {

class TemplateStoreTest : public testing::Test {
  void SetUp() override {
    template_store_ = std::make_unique<TemplateStore>(
        &testing_pref_service_, test_url_loader_factory(), "US");

    // This is set to the Linux Epoch because it is a unittest.
    jan_01_1970_ = base::Time::NowFromSystemTime();

    jan_10_1960_.set_day(10);
    jan_10_1960_.set_month(1);
    jan_10_1960_.set_year(1960);

    jan_10_2001_.set_day(10);
    jan_10_2001_.set_month(1);
    jan_10_2001_.set_year(2001);
  }

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory() {
    return test_url_loader_factory_.GetSafeWeakWrapper();
  }

  void ValidateTemplates(const std::vector<NoteTemplate>& note_templates) {
    std::unordered_set<NoteTemplateIds> ids_set;
    for (const NoteTemplate& note_template : note_templates) {
      EXPECT_LT(NoteTemplateIds::kUnknown, note_template.id());
      EXPECT_GE(NoteTemplateIds::kMaxValue, note_template.id());
      EXPECT_FALSE(note_template.text_style().font_name().empty());

      // There should be no duplicated IDs.
      EXPECT_TRUE(ids_set.find(note_template.id()) == ids_set.end());
      ids_set.insert(note_template.id());
    }
  }

  proto::NoteTemplate GetClassicTemplate() {
    // Background
    proto::Background background;

    background.set_color(0xFF202124);

    /*===========================*/

    // TextStyle
    proto::TextStyle text_style;

    text_style.set_name("Source Serif Pro");
    text_style.set_color(0xFFFFFFFF);
    text_style.set_weight(700);
    text_style.set_allcaps(false);
    text_style.set_alignment(1);
    text_style.set_mintextsize(14);
    text_style.set_maxtextsize(48);

    /*===========================*/

    // FooterStyle
    proto::FooterStyle footer_style;

    footer_style.set_textcolor(0xB3FFFFFF);
    footer_style.set_logocolor(0x33FFFFFF);

    /*===========================*/

    // NoteTemplate
    proto::NoteTemplate classic;

    classic.set_id(1);
    classic.set_allocated_mainbackground(new proto::Background(background));
    classic.set_allocated_textstyle(new proto::TextStyle(text_style));
    classic.set_allocated_footerstyle(new proto::FooterStyle(footer_style));

    return classic;
  }

  proto::NoteTemplate GetFriendlyTemplate() {
    proto::Background background;

    background.set_url(
        "https://www.gstatic.com/chrome/content/"
        "templates/FriendlyBackground@2x.png");

    /*===========================*/

    proto::TextStyle text_style;

    text_style.set_name("Rock Salt");
    text_style.set_color(0xFF202124);
    text_style.set_weight(400);
    text_style.set_allcaps(false);
    text_style.set_alignment(1);
    text_style.set_mintextsize(14);
    text_style.set_maxtextsize(48);

    /*===========================*/

    proto::FooterStyle footer_style;

    footer_style.set_textcolor(0xCC000000);
    footer_style.set_logocolor(0x1A000000);

    /*===========================*/

    proto::NoteTemplate friendly;

    friendly.set_id(2);
    friendly.set_allocated_mainbackground(new proto::Background(background));
    friendly.set_allocated_textstyle(new proto::TextStyle(text_style));
    friendly.set_allocated_footerstyle(new proto::FooterStyle(footer_style));

    return friendly;
  }

  proto::NoteTemplate GetLovelyTemplate() {
    proto::Background background;
    proto::Gradient gradient;

    gradient.set_orientation(2);
    gradient.add_colors(0xFFCEF9FF);
    gradient.add_colors(0xFFF1DFFF);

    background.set_allocated_gradient(new proto::Gradient(gradient));

    proto::Background content_background;

    content_background.set_color(0xFFFFFFFF);

    /*===========================*/

    proto::TextStyle text_style;

    text_style.set_name("Source Serif Pro");
    text_style.set_color(0xFF000000);
    text_style.set_weight(400);
    text_style.set_allcaps(false);
    text_style.set_alignment(2);
    text_style.set_mintextsize(14);
    text_style.set_maxtextsize(48);

    /*===========================*/

    proto::FooterStyle footer_style;

    footer_style.set_textcolor(0xCC000000);
    footer_style.set_logocolor(0x1A000000);

    /*===========================*/

    proto::NoteTemplate lovely;

    lovely.set_id(6);
    lovely.set_allocated_mainbackground(new proto::Background(background));
    lovely.set_allocated_contentbackground(
        new proto::Background(content_background));
    lovely.set_allocated_textstyle(new proto::TextStyle(text_style));
    lovely.set_allocated_footerstyle(new proto::FooterStyle(footer_style));

    return lovely;
  }

  std::string GetOneMaxTemplatesValidProtoString() {
    std::string data;

    proto::Collection collection;
    collection.set_max_template_number(1);

    proto::NoteTemplate classic = GetClassicTemplate();
    proto::NoteTemplate friendly = GetFriendlyTemplate();

    proto::CollectionItem* classic_template = collection.add_collectionitems();
    classic_template->set_allocated_notetemplate(
        new proto::NoteTemplate(classic));
    proto::CollectionItem* friendly_template = collection.add_collectionitems();
    friendly_template->set_allocated_notetemplate(
        new proto::NoteTemplate(friendly));

    collection.SerializeToString(&data);
    return data;
  }

  std::string GetInvalidProtoString() {
    std::string data;

    proto::Collection collection;
    // collection.set_max_template_number() is missing which is required.

    proto::NoteTemplate classic = GetClassicTemplate();
    proto::NoteTemplate friendly = GetFriendlyTemplate();

    proto::CollectionItem* classic_template = collection.add_collectionitems();
    classic_template->set_allocated_notetemplate(
        new proto::NoteTemplate(classic));
    proto::CollectionItem* friendly_template = collection.add_collectionitems();
    friendly_template->set_allocated_notetemplate(
        new proto::NoteTemplate(friendly));

    collection.SerializeToString(&data);
    return data;
  }

  // Have to use TaskEnvironment since the TemplateStore posts tasks to the
  // thread pool.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple testing_pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TemplateStore> template_store_;

  base::Time jan_01_1970_;

  proto::Date jan_10_1960_;
  proto::Date jan_10_2001_;
  proto::Date invalid_date_;
  proto::Collection collection_;
};

// Tests that the store does return templates, and also validates the
// templates' information.
TEST_F(TemplateStoreTest, DefaultTemplates) {
  scoped_feature_list_.InitWithFeatures({kWebNotesStylizeEnabled},
                                        {kWebNotesDynamicTemplates});
  base::RunLoop run_loop;

  template_store_->GetTemplates(base::BindLambdaForTesting(
      [&run_loop, this](std::vector<NoteTemplate> templates) {
        EXPECT_EQ(10U, templates.size());

        ValidateTemplates(templates);

        run_loop.Quit();
      }));

  run_loop.Run();
}

// Test that templates without any dates will still be available.
TEST_F(TemplateStoreTest, NoDates) {
  proto::CollectionItem* no_dates = collection_.add_collectionitems();
  no_dates->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  EXPECT_TRUE(template_store_->TemplateDateAvailable(*no_dates, jan_01_1970_));
}

// Tests that templates with expiration dates that have not expired yet will be
// available.
TEST_F(TemplateStoreTest, ActiveExpiration) {
  proto::CollectionItem* active_expiration = collection_.add_collectionitems();
  active_expiration->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  active_expiration->set_allocated_expiration(new proto::Date(jan_10_2001_));
  EXPECT_TRUE(
      template_store_->TemplateDateAvailable(*active_expiration, jan_01_1970_));
}

// Tests that templates with expired expiration dates will not be available.
TEST_F(TemplateStoreTest, InactiveExpiration) {
  proto::CollectionItem* inactive_expiration =
      collection_.add_collectionitems();
  inactive_expiration->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  inactive_expiration->set_allocated_expiration(new proto::Date(jan_10_1960_));
  EXPECT_FALSE(template_store_->TemplateDateAvailable(*inactive_expiration,
                                                      jan_01_1970_));
}

// Tests that templates with proto::Date objects that are missing fields will
// not be available.
TEST_F(TemplateStoreTest, InvalidDates) {
  proto::CollectionItem* invalid_expiration = collection_.add_collectionitems();
  invalid_expiration->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  invalid_expiration->set_allocated_expiration(new proto::Date(invalid_date_));
  EXPECT_FALSE(template_store_->TemplateDateAvailable(*invalid_expiration,
                                                      jan_01_1970_));

  proto::CollectionItem* invalid_activation = collection_.add_collectionitems();
  invalid_activation->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  invalid_activation->set_allocated_activation(new proto::Date(invalid_date_));
  EXPECT_FALSE(template_store_->TemplateDateAvailable(*invalid_activation,
                                                      jan_01_1970_));
}

// Tests that templates with activation dates that have passed will be
// available.
TEST_F(TemplateStoreTest, ActiveActivation) {
  proto::CollectionItem* active_activation = collection_.add_collectionitems();
  active_activation->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  active_activation->set_allocated_activation(new proto::Date(jan_10_1960_));
  EXPECT_TRUE(
      template_store_->TemplateDateAvailable(*active_activation, jan_01_1970_));
}

// Tests that templates with activation dates that have yet to pass will not be
// available.
TEST_F(TemplateStoreTest, InactiveActivation) {
  proto::CollectionItem* inactive_activation =
      collection_.add_collectionitems();
  inactive_activation->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  inactive_activation->set_allocated_activation(new proto::Date(jan_10_2001_));
  EXPECT_FALSE(template_store_->TemplateDateAvailable(*inactive_activation,
                                                      jan_01_1970_));
}

// Tests that if today's date is within activation and expiration date, it will
// be available.
TEST_F(TemplateStoreTest, BothDatesActive) {
  proto::CollectionItem* both_dates_active = collection_.add_collectionitems();
  both_dates_active->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  both_dates_active->set_allocated_activation(new proto::Date(jan_10_1960_));
  both_dates_active->set_allocated_expiration(new proto::Date(jan_10_2001_));
  EXPECT_TRUE(
      template_store_->TemplateDateAvailable(*both_dates_active, jan_01_1970_));
}

// Tests that if the activation and expiration dates are in the wrong order
// (activation comes after the expiration here), it will not be available.
TEST_F(TemplateStoreTest, BothDatesInactive) {
  proto::CollectionItem* both_dates_inactive =
      collection_.add_collectionitems();
  both_dates_inactive->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  both_dates_inactive->set_allocated_activation(new proto::Date(jan_10_2001_));
  both_dates_inactive->set_allocated_expiration(new proto::Date(jan_10_1960_));
  EXPECT_FALSE(template_store_->TemplateDateAvailable(*both_dates_inactive,
                                                      jan_01_1970_));
}

// Tests that if a date is impossible, such as the 40th day, 13th month, or
// 1000000th year (base::Time::exploded requires year be 4 digits) it will not
// available.
TEST_F(TemplateStoreTest, ImpossibleDates) {
  proto::CollectionItem* invalid_expiration = collection_.add_collectionitems();
  invalid_expiration->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  invalid_date_.set_day(40);
  invalid_date_.set_month(13);
  invalid_date_.set_year(1000000);

  invalid_expiration->set_allocated_expiration(new proto::Date(invalid_date_));
  EXPECT_FALSE(template_store_->TemplateDateAvailable(*invalid_expiration,
                                                      jan_01_1970_));
}

// Tests that it will stop at the set maximum number of templates even if there
// are more templates that are available.
TEST_F(TemplateStoreTest, OneMaxTemplates) {
  test_url_loader_factory_.AddResponse(
      kTemplateUrl, GetOneMaxTemplatesValidProtoString(), net::HTTP_OK);
  scoped_feature_list_.InitWithFeatures(
      {kWebNotesStylizeEnabled, kWebNotesDynamicTemplates}, {});
  base::RunLoop run_loop;

  template_store_->GetTemplates(base::BindLambdaForTesting(
      [&run_loop, this](std::vector<NoteTemplate> templates) {
        EXPECT_EQ(1U, templates.size());
        // Tests to make sure it got the first template and not the second one.
        EXPECT_EQ(static_cast<int>(templates.at(0).id()), 1);

        ValidateTemplates(templates);

        run_loop.Quit();
      }));

  run_loop.Run();
}

// Tests that default templates are available if it is given invalid
// Protobuf data.
TEST_F(TemplateStoreTest, EmptyString) {
  test_url_loader_factory_.AddResponse(kTemplateUrl, "", net::HTTP_OK);
  scoped_feature_list_.InitWithFeatures(
      {kWebNotesStylizeEnabled, kWebNotesDynamicTemplates}, {});
  base::RunLoop run_loop;

  template_store_->GetTemplates(base::BindLambdaForTesting(
      [&run_loop, this](std::vector<NoteTemplate> templates) {
        EXPECT_EQ(10U, templates.size());

        ValidateTemplates(templates);

        run_loop.Quit();
      }));

  run_loop.Run();
}

// Tests that template will be available if it does not have any
// country codes set.
TEST_F(TemplateStoreTest, NoLocation) {
  proto::CollectionItem* no_location = collection_.add_collectionitems();
  no_location->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  EXPECT_TRUE(template_store_->TemplateLocationAvailable(*no_location));
}

// Tests that template will be available if the user's location is
// specified by the template.
TEST_F(TemplateStoreTest, AvailableLocation) {
  proto::CollectionItem* available_location = collection_.add_collectionitems();
  available_location->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  available_location->add_geo("US");

  EXPECT_TRUE(template_store_->TemplateLocationAvailable(*available_location));
}

// Tests that template will not be available if the user's location
// is not specified by the template.
TEST_F(TemplateStoreTest, UnavailableLocation) {
  proto::CollectionItem* unavailable_location =
      collection_.add_collectionitems();
  unavailable_location->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  unavailable_location->add_geo("UK");

  EXPECT_FALSE(
      template_store_->TemplateLocationAvailable(*unavailable_location));
}

// Tests that template will be available if there are multiple
// locations, including the user's.
TEST_F(TemplateStoreTest, MultipleLocations) {
  proto::CollectionItem* multiple_locations = collection_.add_collectionitems();
  multiple_locations->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  multiple_locations->add_geo("UK");
  multiple_locations->add_geo("CA");
  multiple_locations->add_geo("FR");
  multiple_locations->add_geo("US");

  EXPECT_TRUE(template_store_->TemplateLocationAvailable(*multiple_locations));
}

// Tests that template will not be available if the user has an
// undetermined location, but the template has a location.
TEST_F(TemplateStoreTest, UndeterminedLocation) {
  std::unique_ptr<TemplateStore> empty_location =
      std::make_unique<TemplateStore>(&testing_pref_service_,
                                      test_url_loader_factory(), "");

  proto::CollectionItem* undetermined_location =
      collection_.add_collectionitems();
  undetermined_location->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  undetermined_location->add_geo("UK");

  EXPECT_FALSE(
      empty_location->TemplateLocationAvailable(*undetermined_location));
}

// Tests that template will be available if the location is
// unspecified, even if the user has an undetermined location.
TEST_F(TemplateStoreTest, AvailableToAllAndUndeterminedLocation) {
  std::unique_ptr<TemplateStore> empty_location =
      std::make_unique<TemplateStore>(&testing_pref_service_,
                                      test_url_loader_factory(), "");

  proto::CollectionItem* undetermined_location =
      collection_.add_collectionitems();
  undetermined_location->set_allocated_notetemplate(
      new proto::NoteTemplate(GetFriendlyTemplate()));

  EXPECT_TRUE(
      empty_location->TemplateLocationAvailable(*undetermined_location));
}

}  // namespace content_creation

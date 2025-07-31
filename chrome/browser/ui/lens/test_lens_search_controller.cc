// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/test_lens_search_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_contextualization_controller.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/variations_client.h"

namespace lens {

namespace {

// The fake server session id.
constexpr char kTestServerSessionId[] = "server_session_id";

// The fake search session id.
constexpr char kTestSearchSessionId[] = "search_session_id";

// The fake suggest signals.
constexpr char kTestSuggestSignals[] = "encoded_image_signals";

lens::Text CreateTestText(const std::vector<std::string>& words) {
  lens::Text text;
  text.set_content_language("es");
  // Create a paragraph.
  lens::TextLayout::Paragraph* paragraph =
      text.mutable_text_layout()->add_paragraphs();
  // Create a line.
  lens::TextLayout::Line* line = paragraph->add_lines();

  for (size_t i = 0; i < words.size(); ++i) {
    lens::TextLayout::Word* word = line->add_words();
    word->set_plain_text(words[i]);
    word->set_text_separator(" ");
    word->mutable_geometry()->mutable_bounding_box()->set_center_x(0.1 * i);
    word->mutable_geometry()->mutable_bounding_box()->set_center_y(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_width(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_height(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_coordinate_type(
        lens::NORMALIZED);
  }
  return text;
}

}  // namespace

std::unique_ptr<LensOverlayController>
TestLensSearchController::CreateLensOverlayController(
    tabs::TabInterface* tab,
    LensSearchController* lens_search_controller,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service) {
  // Set browser color scheme to light mode for consistency.
  theme_service->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  return std::make_unique<TestLensOverlayController>(
      tab, lens_search_controller, variations_client, identity_manager,
      pref_service, sync_service, theme_service);
}

std::unique_ptr<lens::LensOverlayQueryController>
TestLensSearchController::CreateLensQueryController(
    lens::LensOverlayFullImageResponseCallback full_image_callback,
    lens::LensOverlayUrlResponseCallback url_callback,
    lens::LensOverlayInteractionResponseCallback interaction_callback,
    lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
    lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    lens::UploadProgressCallback upload_progress_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode,
    lens::LensOverlayGen204Controller* gen204_controller) {
  auto fake_query_controller =
      std::make_unique<lens::TestLensOverlayQueryController>(
          full_image_callback, url_callback, interaction_callback,
          suggest_inputs_callback, thumbnail_created_callback,
          upload_progress_callback, variations_client, identity_manager,
          profile, invocation_source, use_dark_mode, gen204_controller);

  // Set up the fake responses for the query controller.
  lens::LensOverlayServerClusterInfoResponse cluster_info_response;
  cluster_info_response.set_server_session_id(kTestServerSessionId);
  cluster_info_response.set_search_session_id(kTestSearchSessionId);
  fake_query_controller->set_fake_cluster_info_response(cluster_info_response);

  lens::LensOverlayObjectsResponse objects_response;
  objects_response.mutable_text()->CopyFrom(
      CreateTestText({"This", "is", "test", "text."}));
  objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  objects_response.mutable_cluster_info()->set_search_session_id(
      kTestSearchSessionId);
  fake_query_controller->set_fake_objects_response(objects_response);

  lens::LensOverlayInteractionResponse interaction_response;
  interaction_response.set_encoded_response(kTestSuggestSignals);
  fake_query_controller->set_fake_interaction_response(interaction_response);
  return fake_query_controller;
}

std::unique_ptr<lens::LensSearchContextualizationController>
TestLensSearchController::CreateLensSearchContextualizationController() {
  return std::make_unique<lens::TestLensSearchContextualizationController>(
      this);
}

}  // namespace lens

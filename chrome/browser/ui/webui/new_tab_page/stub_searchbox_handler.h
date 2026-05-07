// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_STUB_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_STUB_SEARCHBOX_HANDLER_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

#include <string>
#include <vector>

#include "base/unguessable_token.h"
#include "components/omnibox/browser/omnibox.mojom.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class Profile;
namespace content {
class WebUIDataSource;
}

class StubSearchboxHandler : public searchbox::mojom::PageHandler {
 public:
  StubSearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver,
      mojo::PendingRemote<searchbox::mojom::Page> page);

  StubSearchboxHandler(const StubSearchboxHandler&) = delete;
  StubSearchboxHandler& operator=(const StubSearchboxHandler&) = delete;

  ~StubSearchboxHandler() override;

  // searchbox::mojom::PageHandler:
  void OnFocusChanged(bool focused) override;
  void OnThumbnailRemoved() override;
  void QueryAutocomplete(const std::u16string& input,
                         bool prevent_inline_autocomplete) override;
  void StopAutocomplete(bool clear_result) override;
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             uint8_t mouse_button,
                             bool alt_key,
                             bool ctrl_key,
                             bool meta_key,
                             bool shift_key) override;
  void SetPopupSelection(
      searchbox::mojom::OmniboxPopupSelectionPtr selection) override;
  void OpenPopupSelection(uint32_t result_sequence_id,
                          searchbox::mojom::OmniboxPopupSelectionPtr selection,
                          WindowOpenDisposition disposition) override;
  void OnNavigationLikely(
      uint8_t line,
      const GURL& url,
      omnibox::mojom::NavigationPredictor navigation_predictor) override;
  void DeleteAutocompleteMatch(uint8_t line, const GURL& url) override;
  void ActivateKeyword(uint8_t line,
                       const GURL& url,
                       base::TimeTicks match_selection_timestamp,
                       bool is_mouse_event) override;
  void ExecuteAction(uint8_t line,
                     uint8_t action_index,
                     const GURL& url,
                     base::TimeTicks match_selection_timestamp,
                     uint8_t mouse_button,
                     bool alt_key,
                     bool ctrl_key,
                     bool meta_key,
                     bool shift_key) override;
  void GetPlaceholderConfig(GetPlaceholderConfigCallback callback) override;
  void GetRecentTabs(GetRecentTabsCallback callback) override;
  void GetTabPreview(int32_t tab_id, GetTabPreviewCallback callback) override;
  void GetInputState(GetInputStateCallback callback) override;
  void NotifySessionStarted() override;
  void NotifySessionAbandoned() override;
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override;
  void SetSmartComposeStats(
      searchbox::mojom::SmartComposeStatsPtr smart_compose_stats) override;
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     AddTabContextCallback callback) override;
  void OnDriveUploadClicked(OnDriveUploadClickedCallback callback) override;
  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override;
  void ClearFiles(bool should_block_auto_suggested_tabs) override;
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void OpenLensSearch() override;
  void SetActiveToolMode(omnibox::ToolMode tool) override;
  void RecordToolSelectionAction(omnibox::ToolMode tool) override;
  void SetActiveModelMode(omnibox::ModelMode model) override;
  void RecordModelSelectionAction(omnibox::ModelMode model) override;
  void ActivateMetricsFunnel(const std::string& funnel_name) override;
  void ShouldShowDriveDisclaimer(
      ShouldShowDriveDisclaimerCallback callback) override;
  void OnDriveDisclaimerAccepted() override;
  void GetPageClassification(GetPageClassificationCallback callback) override;

  static void SetupWebUIDataSource(content::WebUIDataSource* source,
                                   Profile* profile);

 private:
  mojo::Receiver<searchbox::mojom::PageHandler> receiver_;
  mojo::Remote<searchbox::mojom::Page> page_;
};

#endif  // BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_STUB_SEARCHBOX_HANDLER_H_

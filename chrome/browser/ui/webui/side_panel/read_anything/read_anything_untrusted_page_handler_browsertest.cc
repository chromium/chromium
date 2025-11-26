// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/read_anything/read_anything.mojom-data-view.h"
#include "chrome/common/read_anything/read_anything.mojom-forward.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language_detection/core/constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/mojom/base/values.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/mojom/ax_event.mojom.h"
#include "ui/accessibility/mojom/ax_tree_id.mojom.h"
#include "ui/accessibility/mojom/ax_tree_update.mojom.h"
#include "ui/gfx/geometry/size.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/bind.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/chrome_os_extension_wrapper.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/process_manager.h"
using ash::language_packs::GetPackStateCallback;
using ash::language_packs::OnInstallCompleteCallback;
using ash::language_packs::PackResult;
using read_anything::mojom::InstallationState;
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

using testing::_;

class MockPage : public read_anything::mojom::UntrustedPage {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<read_anything::mojom::UntrustedPage> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD3(AccessibilityEventReceived,
               void(const ui::AXTreeID& tree_id,
                    const std::vector<ui::AXTreeUpdate>& updates,
                    const std::vector<ui::AXEvent>& events));
  MOCK_METHOD(void,
              AccessibilityLocationChangesReceived,
              (const ui::AXTreeID& tree_id,
               ui::AXLocationAndScrollUpdates& details));
  MOCK_METHOD(void,
              AccessibilityLocationChangesReceived,
              (const ui::AXTreeID& tree_id,
               const ui::AXLocationAndScrollUpdates& details));
  MOCK_METHOD(void,
              OnSettingsRestoredFromPrefs,
              (read_anything::mojom::LineSpacing line_spacing,
               read_anything::mojom::LetterSpacing letter_spacing,
               const std::string& font,
               double font_size,
               bool links_enabled,
               bool images_enabled,
               read_anything::mojom::Colors color,
               double speech_rate,
               base::Value::Dict voices,
               base::Value::List languages_enabled_in_pref,
               read_anything::mojom::HighlightGranularity granularity));
  MOCK_METHOD(void,
              OnImageDataDownloaded,
              (const ui::AXTreeID&, int, const SkBitmap&));
  MOCK_METHOD3(OnActiveAXTreeIDChanged,
               void(const ui::AXTreeID& tree_id,
                    ukm::SourceId ukm_source_id,
                    bool is_pdf));
  MOCK_METHOD(void, OnAXTreeDestroyed, (const ui::AXTreeID&));
  MOCK_METHOD(void, SetLanguageCode, (const std::string&));
  MOCK_METHOD(void, SetDefaultLanguageCode, (const std::string&));
  MOCK_METHOD(void, ScreenAIServiceReady, ());
  MOCK_METHOD(void, OnReadingModeHidden, (bool tab_active));
  MOCK_METHOD(void, OnTabWillDetach, ());
  MOCK_METHOD(void, OnTabMuteStateChange, (bool muted));
  MOCK_METHOD(void,
              OnGetVoicePackInfo,
              (read_anything::mojom::VoicePackInfoPtr voice_pack_info));
#if BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD(void, OnDeviceLocked, ());
#else
  MOCK_METHOD(void, OnTtsEngineInstalled, ());
#endif

  mojo::Receiver<read_anything::mojom::UntrustedPage> receiver_{this};
};

#if BUILDFLAG(IS_CHROMEOS)
class MockChromeOsExtensionWrapper : public ChromeOsExtensionWrapper {
 public:
  MockChromeOsExtensionWrapper() = default;
  ~MockChromeOsExtensionWrapper() override = default;

  MOCK_METHOD(void, ActivateSpeechEngine, (Profile * profile));
  MOCK_METHOD(void, ReleaseSpeechEngine, (Profile * profile));
  MOCK_METHOD(void,
              RequestLanguageInfo,
              (const std::string& language, GetPackStateCallback callback));
  MOCK_METHOD(void,
              RequestLanguageInstall,
              (const std::string& language,
               OnInstallCompleteCallback callback));
};
#endif

class TestReadAnythingUntrustedPageHandler
    : public ReadAnythingUntrustedPageHandler {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  explicit TestReadAnythingUntrustedPageHandler(
      mojo::PendingRemote<read_anything::mojom::UntrustedPage> page,
      content::WebUI* test_web_ui,
      std::unique_ptr<ChromeOsExtensionWrapper> extension_wrapper)
      : ReadAnythingUntrustedPageHandler(
            std::move(page),
            mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandler>(),
            test_web_ui,
            /*use_screen_ai_service=*/false,
            std::move(extension_wrapper)) {}
#else
  explicit TestReadAnythingUntrustedPageHandler(
      mojo::PendingRemote<read_anything::mojom::UntrustedPage> page,
      content::WebUI* test_web_ui)
      : ReadAnythingUntrustedPageHandler(
            std::move(page),
            mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandler>(),
            test_web_ui,
            /*use_screen_ai_service=*/false) {}
#endif
  void OnImageDataRequested(const ui::AXTreeID& target_tree_id,
                            ui::AXNodeID target_node_id) override {
    OnImageDataDownloaded(target_tree_id, target_node_id, /*id=*/0,
                          /*http_status_code=*/0, GURL(),
                          /*bitmaps=*/{test_bitmap_},
                          /*sizes=*/{gfx::Size(10, 10)});
  }

  void SetTestBitmap(SkBitmap bitmap) { test_bitmap_ = bitmap; }

 private:
  SkBitmap test_bitmap_;
};

class FakeTtsEngineDelegate : public content::TtsEngineDelegate {
 public:
  void Speak(content::TtsUtterance* utterance,
             const content::VoiceData& voice) override {}
  void LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override {
  }
  bool IsBuiltInTtsEngineInitialized(
      content::BrowserContext* browser_context) override {
    return true;
  }
  void GetVoices(content::BrowserContext* browser_context,
                 const GURL& source_url,
                 std::vector<content::VoiceData>* out_voices) override {}
  void Stop(content::TtsUtterance* utterance) override {}
  void Pause(content::TtsUtterance* utterance) override {}
  void Resume(content::TtsUtterance* utterance) override {}

  void LanguageStatusRequest(content::BrowserContext* browser_context,
                             const std::string& lang,
                             const std::string& client_id,
                             int source) override {
    last_requested_status_ = lang;
  }
  void UninstallLanguageRequest(content::BrowserContext* browser_context,
                                const std::string& lang,
                                const std::string& client_id,
                                int source,
                                bool uninstall_immediately) override {
    last_requested_uninstall_ = lang;
  }
  void InstallLanguageRequest(content::BrowserContext* browser_context,
                              const std::string& lang,
                              const std::string& client_id,
                              int source) override {
    last_requested_install_ = lang;
  }

  std::string last_requested_status() { return last_requested_status_; }
  std::string last_requested_install() { return last_requested_install_; }
  std::string last_requested_uninstall() { return last_requested_uninstall_; }

 private:
  std::string last_requested_status_ = "";
  std::string last_requested_install_ = "";
  std::string last_requested_uninstall_ = "";
};

// TODO: b/40927698 - Add more tests.
class ReadAnythingUntrustedPageHandlerTest : public InProcessBrowserTest {
 public:
  ReadAnythingUntrustedPageHandlerTest() {
    // FeatureList overrides must happen in the constructor or SetUp (before
    // base::SetUp).
    scoped_feature_list_.InitAndEnableFeature(
        {features::kReadAnythingReadAloud});
    // `TestReadAnythingUntrustedPageHandler` disables ScreenAI service, which
    // disables using ReadAnythingWithScreen2x and PdfOcr.
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(web_contents_.get());

    // Normally this would be done by ReadAnythingSidePanelControllerGlue as it
    // creates the WebView, but this unit test skips that step.
    ReadAnythingSidePanelControllerGlue::CreateForWebContents(
        web_contents_.get(), browser()
                                 ->GetActiveTabInterface()
                                 ->GetTabFeatures()
                                 ->read_anything_side_panel_controller());
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS)
    extension_wrapper_ptr_ = nullptr;
#endif
    handler_.reset();
    test_web_ui_.reset();
    web_contents_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<TestReadAnythingUntrustedPageHandler> CreateHandler() {
#if BUILDFLAG(IS_CHROMEOS)
    std::unique_ptr<ChromeOsExtensionWrapper> extension_wrapper_mock =
        std::make_unique<testing::NiceMock<MockChromeOsExtensionWrapper>>();
    extension_wrapper_ptr_ = static_cast<MockChromeOsExtensionWrapper*>(
        extension_wrapper_mock.get());
    return std::make_unique<TestReadAnythingUntrustedPageHandler>(
        page_.BindAndGetRemote(), test_web_ui_.get(),
        std::move(extension_wrapper_mock));
#else
    return std::make_unique<TestReadAnythingUntrustedPageHandler>(
        page_.BindAndGetRemote(), test_web_ui_.get());
#endif
  }

  ReadAnythingSidePanelController* side_panel_controller() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->read_anything_side_panel_controller();
  }

  SidePanelEntry* read_anything_entry() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->side_panel_registry()
        ->GetEntryForKey(
            SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  }

  ChromeTranslateClient* GetChromeTranslateClient() {
    return ChromeTranslateClient::FromWebContents(
        browser()
            ->GetActiveTabInterface()
            ->GetTabFeatures()
            ->read_anything_side_panel_controller()
            ->tab()
            ->GetContents());
  }

  void SetTranslateSourceLanguage(const std::string& language) {
    GetChromeTranslateClient()
        ->GetTranslateManager()
        ->GetLanguageState()
        ->SetSourceLanguage(language);
  }

  bool HasAudio() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->read_anything_side_panel_controller()
        ->tab()
        ->GetContents()
        ->IsCurrentlyAudible();
  }

  void OnLineSpaceChange(read_anything::mojom::LineSpacing line_spacing) {
    handler_->OnLineSpaceChange(line_spacing);
  }

  void OnLetterSpaceChange(read_anything::mojom::LetterSpacing letter_spacing) {
    handler_->OnLetterSpaceChange(letter_spacing);
  }

  void OnFontChange(const std::string& font) { handler_->OnFontChange(font); }

  void OnFontSizeChange(double font_size) {
    handler_->OnFontSizeChange(font_size);
  }

  void OnLinksEnabledChanged(bool enabled) {
    handler_->OnLinksEnabledChanged(enabled);
  }

  void OnImagesEnabledChanged(bool enabled) {
    handler_->OnImagesEnabledChanged(enabled);
  }

  void OnColorChange(read_anything::mojom::Colors color) {
    handler_->OnColorChange(color);
  }

  void OnHighlightGranularityChanged(
      read_anything::mojom::HighlightGranularity granularity) {
    handler_->OnHighlightGranularityChanged(granularity);
  }

  void OnVoiceChange(const std::string& voice, const std::string& lang) {
    handler_->OnVoiceChange(voice, lang);
  }

  void OnLanguagePrefChange(const std::string& lang, bool enabled) {
    handler_->OnLanguagePrefChange(lang, enabled);
  }

  void OnSpeechRateChange(double rate) { handler_->OnSpeechRateChange(rate); }

  void OnTabWillDetach() { handler_->OnTabWillDetach(); }

  void Activate(bool active, SidePanelOpenTrigger* trigger = nullptr) {
    SidePanelEntry* entry = read_anything_entry();
    if (trigger) {
      entry->set_last_open_trigger(*trigger);
    }
    if (active) {
      side_panel_controller()->OnEntryShown(entry);
    } else {
      side_panel_controller()->OnEntryHidden(entry);
    }
  }

  void OnImageDataRequested(const ui::AXTreeID& target_tree_id,
                            ui::AXNodeID target_node_id) {
    handler_->OnImageDataRequested(target_tree_id, target_node_id);
  }

  void OnLanguageDetermined(const std::string& code) {
    translate::LanguageDetectionDetails details;
    details.adopted_language = code;
    handler_->OnLanguageDetermined(details);
  }

  void GetVoicePackInfo(const std::string& language) {
    handler_->GetVoicePackInfo(language);
  }

  void InstallVoicePack(const std::string& language) {
    handler_->InstallVoicePack(language);
  }

  void UninstallVoice(const std::string& language) {
    handler_->UninstallVoice(language);
  }

  void AccessibilityEventReceived(const ui::AXUpdatesAndEvents& details) {
    handler_->AccessibilityEventReceived(details);
  }

  void OnActiveAXTreeIDChanged() { handler_->OnActiveAXTreeIDChanged(); }

  void OnTranslateDriverDestroyed(translate::TranslateDriver* driver) {
    handler_->OnTranslateDriverDestroyed(driver);
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<MockChromeOsExtensionWrapper> extension_wrapper_ptr_ = nullptr;
#endif
  testing::NiceMock<MockPage> page_;
  FakeTtsEngineDelegate engine_delegate_;
  std::unique_ptr<ReadAnythingUntrustedPageHandler> handler_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnHandlerConstructed_SendsStoredPrefs) {
  read_anything::mojom::LineSpacing expected_line_spacing =
      read_anything::mojom::LineSpacing::kVeryLoose;
  read_anything::mojom::LetterSpacing expected_letter_spacing =
      read_anything::mojom::LetterSpacing::kWide;
  std::string expected_font_name = "Google Sans";
  double expected_font_scale = 3.5;
  bool expected_links_enabled = false;
  bool expected_images_enabled = true;
  read_anything::mojom::Colors expected_color =
      read_anything::mojom::Colors::kBlue;
  double expected_speech_rate = 1.0;
  read_anything::mojom::HighlightGranularity expected_highlight_granularity =
      read_anything::mojom::HighlightGranularity::kDefaultValue;
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(prefs::kAccessibilityReadAnythingLineSpacing, 3);
  prefs->SetInteger(prefs::kAccessibilityReadAnythingLetterSpacing, 2);
  prefs->SetString(prefs::kAccessibilityReadAnythingFontName,
                   expected_font_name);
  prefs->SetDouble(prefs::kAccessibilityReadAnythingFontScale,
                   expected_font_scale);
  prefs->SetBoolean(prefs::kAccessibilityReadAnythingLinksEnabled,
                    expected_links_enabled);
  prefs->SetBoolean(prefs::kAccessibilityReadAnythingImagesEnabled,
                    expected_images_enabled);
  prefs->SetInteger(prefs::kAccessibilityReadAnythingColorInfo, 4);

  EXPECT_CALL(
      page_,
      OnSettingsRestoredFromPrefs(
          expected_line_spacing, expected_letter_spacing, expected_font_name,
          expected_font_scale, expected_links_enabled, expected_images_enabled,
          expected_color, expected_speech_rate, testing::IsEmpty(),
          testing::IsEmpty(), expected_highlight_granularity))
      .Times(1);

  handler_ = CreateHandler();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnLineSpaceChange) {
  const read_anything::mojom::LineSpacing kSpacing1 =
      read_anything::mojom::LineSpacing::kLoose;
  const read_anything::mojom::LineSpacing kSpacing2 =
      read_anything::mojom::LineSpacing::kStandard;
  handler_ = CreateHandler();

  OnLineSpaceChange(kSpacing1);
  int spacing1 = browser()->profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingLineSpacing);
  ASSERT_EQ(spacing1, static_cast<int>(kSpacing1));

  OnLineSpaceChange(kSpacing2);
  int spacing2 = browser()->profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingLineSpacing);
  ASSERT_EQ(spacing2, static_cast<int>(kSpacing2));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnLetterSpaceChange) {
  const read_anything::mojom::LetterSpacing kSpacing1 =
      read_anything::mojom::LetterSpacing::kVeryWide;
  const read_anything::mojom::LetterSpacing kSpacing2 =
      read_anything::mojom::LetterSpacing::kStandard;
  handler_ = CreateHandler();

  OnLetterSpaceChange(kSpacing1);
  const int spacing1 = browser()->profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingLetterSpacing);
  ASSERT_EQ(spacing1, static_cast<int>(kSpacing1));

  OnLetterSpaceChange(kSpacing2);
  const int spacing2 = browser()->profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingLetterSpacing);
  ASSERT_EQ(spacing2, static_cast<int>(kSpacing2));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, OnColorChange) {
  const read_anything::mojom::Colors kColor1 =
      read_anything::mojom::Colors::kBlue;
  const read_anything::mojom::Colors kColor2 =
      read_anything::mojom::Colors::kDark;
  handler_ = CreateHandler();

  OnColorChange(kColor1);
  const int spacing1 = browser()->profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingColorInfo);
  ASSERT_EQ(spacing1, static_cast<int>(kColor1));

  OnColorChange(kColor2);
  const int spacing2 = browser()->profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingColorInfo);
  ASSERT_EQ(spacing2, static_cast<int>(kColor2));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnHighlightGranularityChanged) {
  const read_anything::mojom::HighlightGranularity kGranularity1 =
      read_anything::mojom::HighlightGranularity::kPhrase;
  const read_anything::mojom::HighlightGranularity kGranularity2 =
      read_anything::mojom::HighlightGranularity::kOff;
  handler_ = CreateHandler();

  OnHighlightGranularityChanged(kGranularity1);
  const int granularity1 = browser()->profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingHighlightGranularity);
  ASSERT_EQ(granularity1, static_cast<int>(kGranularity1));

  OnHighlightGranularityChanged(kGranularity2);
  const int granularity2 = browser()->profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingHighlightGranularity);
  ASSERT_EQ(granularity2, static_cast<int>(kGranularity2));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, OnFontChange) {
  const char kFont1[] = "Atkinson Hyperlegible Next";
  const char kFont2[] = "Arial";
  handler_ = CreateHandler();

  OnFontChange(kFont1);
  const std::string font1 = browser()->profile()->GetPrefs()->GetString(
      prefs::kAccessibilityReadAnythingFontName);
  ASSERT_EQ(font1, kFont1);

  OnFontChange(kFont2);
  const std::string font2 = browser()->profile()->GetPrefs()->GetString(
      prefs::kAccessibilityReadAnythingFontName);
  ASSERT_EQ(font2, kFont2);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, OnFontSizeChange) {
  const double kFontSize1 = 2;
  const double kFontSize2 = .5;
  handler_ = CreateHandler();

  OnFontSizeChange(kFontSize1);
  const double fontSize1 = browser()->profile()->GetPrefs()->GetDouble(
      prefs::kAccessibilityReadAnythingFontScale);
  ASSERT_EQ(fontSize1, kFontSize1);

  OnFontSizeChange(kFontSize2);
  const double fontSize2 = browser()->profile()->GetPrefs()->GetDouble(
      prefs::kAccessibilityReadAnythingFontScale);
  ASSERT_EQ(fontSize2, kFontSize2);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnLinksEnabledChanged) {
  handler_ = CreateHandler();

  OnLinksEnabledChanged(true);
  const double fontSize1 = browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityReadAnythingLinksEnabled);
  ASSERT_TRUE(fontSize1);

  OnLinksEnabledChanged(false);
  const double fontSize2 = browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityReadAnythingLinksEnabled);
  ASSERT_FALSE(fontSize2);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnImagesEnabledChanged) {
  handler_ = CreateHandler();

  OnImagesEnabledChanged(true);
  const double fontSize1 = browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityReadAnythingImagesEnabled);
  ASSERT_TRUE(fontSize1);

  OnImagesEnabledChanged(false);
  const double fontSize2 = browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityReadAnythingImagesEnabled);
  ASSERT_FALSE(fontSize2);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnSpeechRateChange) {
  const double kRate1 = 1.5;
  const double kRate2 = .8;
  handler_ = CreateHandler();

  OnSpeechRateChange(kRate1);
  const double rate1 = browser()->profile()->GetPrefs()->GetDouble(
      prefs::kAccessibilityReadAnythingSpeechRate);
  ASSERT_EQ(rate1, kRate1);

  OnSpeechRateChange(kRate2);
  const double rate2 = browser()->profile()->GetPrefs()->GetDouble(
      prefs::kAccessibilityReadAnythingSpeechRate);
  ASSERT_EQ(rate2, kRate2);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnLanguagePrefChange_StoresEnabledLangsInPrefs) {
  const char kLang1[] = "en-au";
  const char kLang2[] = "en-gb";
  const char kDisabledLang[] = "en-us";
  handler_ = CreateHandler();

  OnLanguagePrefChange(kLang1, true);
  OnLanguagePrefChange(kLang2, true);
  OnLanguagePrefChange(kDisabledLang, false);

  const base::Value::List* langs = &browser()->profile()->GetPrefs()->GetList(
      prefs::kAccessibilityReadAnythingLanguagesEnabled);
  ASSERT_EQ(langs->size(), 2u);
  ASSERT_EQ((*langs)[0].GetString(), kLang1);
  ASSERT_EQ((*langs)[1].GetString(), kLang2);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnLanguagePrefChange_SameLang_StoresLatestInPrefs) {
  const char kLang[] = "bn";
  handler_ = CreateHandler();
  PrefService* prefs = browser()->profile()->GetPrefs();

  OnLanguagePrefChange(kLang, true);
  ASSERT_EQ(
      prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled).size(),
      1u);

  OnLanguagePrefChange(kLang, false);
  ASSERT_TRUE(prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled)
                  .empty());

  OnLanguagePrefChange(kLang, true);
  ASSERT_EQ(
      prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled).size(),
      1u);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnLanguagePrefChange_SameLang_StoresOnce) {
  const char kLang[] = "bn";
  handler_ = CreateHandler();
  PrefService* prefs = browser()->profile()->GetPrefs();

  OnLanguagePrefChange(kLang, true);
  ASSERT_EQ(
      prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled).size(),
      1u);

  OnLanguagePrefChange(kLang, true);
  ASSERT_EQ(
      prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled).size(),
      1u);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingUntrustedPageHandlerTest,
    OnHandlerConstructed_WithReadAloud_SendsStoredReadAloudInfo) {
  // Build the voice and lang info.
  const char kLang1[] = "en";
  const char kLang2[] = "fr";
  const char kLang3[] = "it";
  const char kVoice1[] = "Rapunzel";
  const char kVoice2[] = "Eugene";
  const char kVoice3[] = "Cassandra";
  base::Value::Dict voices = base::Value::Dict()
                                 .Set(kLang1, kVoice1)
                                 .Set(kLang2, kVoice2)
                                 .Set(kLang3, kVoice3);
  base::Value::List langs;
  langs.Append(kLang1);
  langs.Append(kLang2);
  langs.Append(kLang3);

  // Set the values in prefs.
  double expected_speech_rate = 1.2;
  read_anything::mojom::HighlightGranularity expected_highlight_granularity =
      read_anything::mojom::HighlightGranularity::kOff;
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetDouble(prefs::kAccessibilityReadAnythingSpeechRate,
                   expected_speech_rate);
  prefs->SetDict(prefs::kAccessibilityReadAnythingVoiceName, std::move(voices));
  prefs->SetList(prefs::kAccessibilityReadAnythingLanguagesEnabled,
                 std::move(langs));
  prefs->SetInteger(prefs::kAccessibilityReadAnythingHighlightGranularity, 1);

  // Verify the values passed to the page are correct.
  EXPECT_CALL(page_, OnSettingsRestoredFromPrefs(
                         _, _, _, _, _, _, _, expected_speech_rate, _, _,
                         expected_highlight_granularity))
      .Times(1)
      .WillOnce(testing::WithArgs<8, 9>(
          [&](base::Value::Dict voices, base::Value::List langs) {
            EXPECT_THAT(voices, base::test::DictionaryHasValues(
                                    base::Value::Dict()
                                        .Set(kLang1, kVoice1)
                                        .Set(kLang2, kVoice2)
                                        .Set(kLang3, kVoice3)));
            EXPECT_EQ(3u, langs.size());
            EXPECT_EQ(langs[0].GetString(), kLang1);
            EXPECT_EQ(langs[1].GetString(), kLang2);
            EXPECT_EQ(langs[2].GetString(), kLang3);
          }));

  handler_ = CreateHandler();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnVoiceChange_StoresInPrefs) {
  const char kLang1[] = "hi";
  const char kLang2[] = "ja";
  const char kVoice1[] = "Ariel";
  const char kVoice2[] = "Sebastian";
  handler_ = CreateHandler();

  OnVoiceChange(kVoice1, kLang1);
  OnVoiceChange(kVoice2, kLang2);

  const base::Value::Dict* voices = &browser()->profile()->GetPrefs()->GetDict(
      prefs::kAccessibilityReadAnythingVoiceName);
  ASSERT_EQ(voices->size(), 2u);
  EXPECT_THAT(
      *voices,
      base::test::DictionaryHasValues(
          base::Value::Dict().Set(kLang1, kVoice1).Set(kLang2, kVoice2)));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnVoiceChange_SameLang_StoresLatestInPrefs) {
  const char kLang[] = "es-es";
  const char kVoice1[] = "Simba";
  const char kVoice2[] = "Nala";
  handler_ = CreateHandler();

  OnVoiceChange(kVoice1, kLang);
  OnVoiceChange(kVoice2, kLang);

  const base::Value::Dict* voices = &browser()->profile()->GetPrefs()->GetDict(
      prefs::kAccessibilityReadAnythingVoiceName);
  ASSERT_EQ(voices->size(), 1u);
  EXPECT_THAT(*voices,
              base::test::DictionaryHasValue(kLang, base::Value(kVoice2)));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnVoiceChange_SameVoiceDifferentLang_StoresBothInPrefs) {
  const char kLang1[] = "pt-pt";
  const char kLang2[] = "pt-br";
  const char kVoice[] = "Peter Parker";
  handler_ = CreateHandler();

  OnVoiceChange(kVoice, kLang1);
  OnVoiceChange(kVoice, kLang2);

  const base::Value::Dict* voices = &browser()->profile()->GetPrefs()->GetDict(
      prefs::kAccessibilityReadAnythingVoiceName);
  ASSERT_EQ(voices->size(), 2u);
  EXPECT_THAT(*voices,
              base::test::DictionaryHasValues(
                  base::Value::Dict().Set(kLang1, kVoice).Set(kLang2, kVoice)));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, BadImageData) {
  auto test_handler_u_ptr = CreateHandler();
  auto* test_handler = test_handler_u_ptr.get();
  handler_ = std::move(test_handler_u_ptr);
  auto tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXNodeID node_id = 1;
  SkBitmap bitmap;
  test_handler->SetTestBitmap(bitmap);
  OnImageDataRequested(tree_id, node_id);
  EXPECT_CALL(page_, OnImageDataDownloaded(_, _, _)).Times(0);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnLanguageDetermined_SendsCodeToPage) {
  const char kLang1[] = "id-id";
  const char kLang2[] = "es-us";
  handler_ = CreateHandler();
  EXPECT_CALL(page_, SetLanguageCode("en")).Times(1);

  OnLanguageDetermined(kLang1);
  OnLanguageDetermined(kLang2);

  EXPECT_CALL(page_, SetLanguageCode(kLang1)).Times(1);
  EXPECT_CALL(page_, SetLanguageCode(kLang2)).Times(1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnLanguageDetermined_SameCodeOnlySentOnce) {
  const char kLang1[] = "id-id";
  handler_ = CreateHandler();
  EXPECT_CALL(page_, SetLanguageCode("en")).Times(1);

  OnLanguageDetermined(kLang1);
  OnLanguageDetermined(kLang1);
  OnLanguageDetermined(kLang1);

  EXPECT_CALL(page_, SetLanguageCode(kLang1)).Times(1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnLanguageDetermined_UnknownLanguageSendsEmpty) {
  handler_ = CreateHandler();
  EXPECT_CALL(page_, SetLanguageCode).Times(1);

  OnLanguageDetermined(language_detection::kUnknownLanguageCode);

  EXPECT_CALL(page_, SetLanguageCode("")).Times(1);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingUntrustedPageHandlerTest,
    OnLanguageDetermined_UnknownLanguageSendsEmptyEveryTime) {
  handler_ = CreateHandler();
  EXPECT_CALL(page_, SetLanguageCode).Times(1);

  OnLanguageDetermined(language_detection::kUnknownLanguageCode);
  OnLanguageDetermined(language_detection::kUnknownLanguageCode);
  OnLanguageDetermined(language_detection::kUnknownLanguageCode);

  EXPECT_CALL(page_, SetLanguageCode("")).Times(3);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       AccessibilityEventReceived) {
  ui::AXUpdatesAndEvents details;
  details.events = {};
  details.updates = {};
  details.ax_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  handler_ = CreateHandler();

  AccessibilityEventReceived(details);

  EXPECT_CALL(page_, AccessibilityEventReceived(details.ax_tree_id, _, _))
      .Times(1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnActiveAXTreeIDChanged) {
  handler_ = CreateHandler();

  OnActiveAXTreeIDChanged();

  // This is called once during construction, so we check for 2 calls here.
  EXPECT_CALL(page_, OnActiveAXTreeIDChanged).Times(2);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnActiveAXTreeIDChanged_SendsExistingLanguageCode) {
  const char kLang[] = "pt-br";
  SetTranslateSourceLanguage(kLang);

  handler_ = CreateHandler();

  // Sets the default language code.
  EXPECT_CALL(page_, SetLanguageCode).Times(1);
  // Sends the detected language code.
  EXPECT_CALL(page_, SetLanguageCode(kLang)).Times(1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnActiveAXTreeIDChanged_SendsNewLanguageCode) {
  handler_ = CreateHandler();
  // The default language code.
  EXPECT_CALL(page_, SetLanguageCode).Times(1);
  const char kLang1[] = "pt-br";
  const char kLang2[] = "bd";

  // Send a new language code.
  SetTranslateSourceLanguage(kLang1);
  OnActiveAXTreeIDChanged();

  EXPECT_CALL(page_, SetLanguageCode(kLang1)).Times(1);

  // Send another language code.
  SetTranslateSourceLanguage(kLang2);
  OnActiveAXTreeIDChanged();

  EXPECT_CALL(page_, SetLanguageCode(kLang2)).Times(1);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingUntrustedPageHandlerTest,
    OnActiveAXTreeIDChanged_AfterTranslateDriverDestroyed_StillSendsLanguage) {
  const char kLang1[] = "pt-br";
  const char kLang2[] = "es-es";
  SetTranslateSourceLanguage(kLang1);
  handler_ = CreateHandler();
  EXPECT_CALL(page_, SetLanguageCode).Times(1);
  EXPECT_CALL(page_, SetLanguageCode(kLang1)).Times(1);

  OnTranslateDriverDestroyed(GetChromeTranslateClient()->GetTranslateDriver());
  SetTranslateSourceLanguage(kLang2);
  OnActiveAXTreeIDChanged();

  EXPECT_CALL(page_, SetLanguageCode(kLang2)).Times(1);
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, GetVoicePackInfo) {
  const char kLang1[] = "id-id";
  const char kLang2[] = "en-gb";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  GetVoicePackInfo(kLang1);
  ASSERT_EQ(kLang1, engine_delegate_.last_requested_status());

  GetVoicePackInfo(kLang2);
  ASSERT_EQ(kLang2, engine_delegate_.last_requested_status());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, InstallVoicePack) {
  const char kLang1[] = "fr-fr";
  const char kLang2[] = "en-us";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  InstallVoicePack(kLang1);
  ASSERT_EQ(kLang1, engine_delegate_.last_requested_install());

  InstallVoicePack(kLang2);
  ASSERT_EQ(kLang2, engine_delegate_.last_requested_install());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, UninstallVoice) {
  const char kLang1[] = "it-it";
  const char kLang2[] = "en-au";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  UninstallVoice(kLang1);
  ASSERT_EQ(kLang1, engine_delegate_.last_requested_uninstall());

  UninstallVoice(kLang2);
  ASSERT_EQ(kLang2, engine_delegate_.last_requested_uninstall());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnUpdateLanguageStatus_NotInstalled) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      GetProfile(), kLang, content::LanguageInstallStatus::NOT_INSTALLED, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(
          testing::WithArg<0>([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kNotInstalled,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnUpdateLanguageStatus_Installing) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      GetProfile(), kLang, content::LanguageInstallStatus::INSTALLING, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(
          testing::WithArg<0>([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kInstalling,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnUpdateLanguageStatus_Installed) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      GetProfile(), kLang, content::LanguageInstallStatus::INSTALLED, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(
          testing::WithArg<0>([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kInstalled,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnUpdateLanguageStatus_Failed) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      GetProfile(), kLang, content::LanguageInstallStatus::FAILED, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(
          testing::WithArg<0>([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kUnknown,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnUpdateLanguageStatus_Unknown) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      GetProfile(), kLang, content::LanguageInstallStatus::UNKNOWN, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(
          testing::WithArg<0>([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kUnknown,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnUpdateLanguageStatus_DifferentProfiles) {
  const char kLang[] = "it-it";
  Profile* profile1 = GetProfile();
  Profile* profile2 =
      InProcessBrowserTest::CreateIncognitoBrowser()->GetProfile();
  Profile* profile3 = InProcessBrowserTest::CreateGuestBrowser()->GetProfile();
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      profile1, kLang, content::LanguageInstallStatus::NOT_INSTALLED, "");
  content::TtsController::GetInstance()->UpdateLanguageStatus(
      profile2, kLang, content::LanguageInstallStatus::INSTALLED, "");
  content::TtsController::GetInstance()->UpdateLanguageStatus(
      profile3, kLang, content::LanguageInstallStatus::FAILED, "");

  // Only forward the language status received for this profile.
  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(
          testing::WithArg<0>([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kNotInstalled,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnUpdateLanguageStatus_IncognitoProfile) {
  const char kLang[] = "en-au";
  Profile* profile1 = GetProfile();
  Profile* profile2 =
      InProcessBrowserTest::CreateIncognitoBrowser()->GetProfile();
  // Assign the incognito contents to be the contents for this handler.
  web_contents_.reset();
  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile2));
  test_web_ui_->set_web_contents(web_contents_.get());
  ReadAnythingSidePanelControllerGlue::CreateForWebContents(
      web_contents_.get(), browser()
                               ->GetActiveTabInterface()
                               ->GetTabFeatures()
                               ->read_anything_side_panel_controller());
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      profile1, kLang, content::LanguageInstallStatus::NOT_INSTALLED, "");
  content::TtsController::GetInstance()->UpdateLanguageStatus(
      profile2, kLang, content::LanguageInstallStatus::INSTALLED, "");

  // Forward both statuses since this handler is for an incognito profile.
  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .Times(2)
      .WillOnce(
          testing::WithArg<0>([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kNotInstalled,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          }))
      .WillOnce(
          testing::WithArg<0>([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kInstalled,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          }));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnUpdateLanguageStatus_GuestProfile) {
  const char kLang[] = "en-au";
  Profile* profile1 = GetProfile();
  Profile* profile2 = InProcessBrowserTest::CreateGuestBrowser()->GetProfile();
  // Assign the guest contents to be the contents for this handler.
  web_contents_.reset();
  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile2));
  test_web_ui_->set_web_contents(web_contents_.get());
  ReadAnythingSidePanelControllerGlue::CreateForWebContents(
      web_contents_.get(), browser()
                               ->GetActiveTabInterface()
                               ->GetTabFeatures()
                               ->read_anything_side_panel_controller());
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = CreateHandler();

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      profile1, kLang, content::LanguageInstallStatus::NOT_INSTALLED, "");
  content::TtsController::GetInstance()->UpdateLanguageStatus(
      profile2, kLang, content::LanguageInstallStatus::INSTALLED, "");

  // Only the status sent on the guest profile should be sent.
  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(
          testing::WithArg<0>([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kInstalled,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          }));
}
#else
IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       Constructor_ActivatesSpeechEngine) {
  auto extension_wrapper_mock =
      std::make_unique<testing::NiceMock<MockChromeOsExtensionWrapper>>();

  extension_wrapper_ptr_ =
      static_cast<MockChromeOsExtensionWrapper*>(extension_wrapper_mock.get());

  EXPECT_CALL(*extension_wrapper_ptr_, ActivateSpeechEngine).Times(1);

  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get(),
      std::move(extension_wrapper_mock));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       Destructor_ReleasesSpeechEngine) {
  handler_ = CreateHandler();

  EXPECT_CALL(*extension_wrapper_ptr_, ReleaseSpeechEngine).Times(1);
  extension_wrapper_ptr_ = nullptr;
  handler_.reset();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, GetVoicePackInfo) {
  const char kLang[] = "en-us";
  PackResult result;
  result.pack_state = PackResult::StatusCode::kInProgress;
  result.operation_error = PackResult::ErrorCode::kNone;
  result.language_code = kLang;

  base::RunLoop run_loop;
  handler_ = CreateHandler();

  EXPECT_CALL(*extension_wrapper_ptr_, RequestLanguageInfo)
      .WillOnce(
          [&](const std::string& language, GetPackStateCallback callback) {
            EXPECT_EQ(kLang, language);
            std::move(callback).Run(result);
          });
  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce([&](read_anything::mojom::VoicePackInfoPtr info) {
        EXPECT_EQ(kLang, info->language);
        EXPECT_EQ(InstallationState::kInstalling,
                  info->pack_state->get_installation_state());
        run_loop.Quit();
      });

  GetVoicePackInfo(kLang);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       GetVoicePackInfo_SendsErrorResult) {
  const char kLang[] = "en-us";
  PackResult result;
  result.pack_state = PackResult::StatusCode::kUnknown;
  result.operation_error = PackResult::ErrorCode::kWrongId;
  result.language_code = kLang;

  base::RunLoop run_loop;
  handler_ = CreateHandler();

  ON_CALL(*extension_wrapper_ptr_, RequestLanguageInfo)
      .WillByDefault(
          [&](const std::string& language, GetPackStateCallback callback) {
            std::move(callback).Run(result);
          });
  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce([&](read_anything::mojom::VoicePackInfoPtr info) {
        EXPECT_EQ(kLang, info->language);
        EXPECT_EQ(read_anything::mojom::ErrorCode::kWrongId,
                  info->pack_state->get_error_code());
        run_loop.Quit();
      });

  GetVoicePackInfo(kLang);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, InstallVoicePack) {
  const char kLang[] = "en-us";
  PackResult result;
  result.pack_state = PackResult::StatusCode::kInstalled;
  result.operation_error = PackResult::ErrorCode::kNone;
  result.language_code = kLang;

  base::RunLoop run_loop;
  handler_ = CreateHandler();

  EXPECT_CALL(*extension_wrapper_ptr_, RequestLanguageInstall)
      .WillOnce(
          [&](const std::string& language, OnInstallCompleteCallback callback) {
            EXPECT_EQ(language, kLang);
            std::move(callback).Run(result);
          });
  EXPECT_CALL(*extension_wrapper_ptr_, RequestLanguageInfo)
      .WillOnce(
          [&](const std::string& language, GetPackStateCallback callback) {
            EXPECT_EQ(language, kLang);
            std::move(callback).Run(result);
          });
  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce([&](read_anything::mojom::VoicePackInfoPtr info) {
        EXPECT_EQ(kLang, info->language);
        EXPECT_EQ(InstallationState::kInstalled,
                  info->pack_state->get_installation_state());
        run_loop.Quit();
      });

  InstallVoicePack(kLang);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       InstallVoicePack_SendsErrorResult) {
  const char kLang[] = "en-us";
  PackResult result;
  result.pack_state = PackResult::StatusCode::kUnknown;
  result.operation_error = PackResult::ErrorCode::kWrongId;
  result.language_code = kLang;

  base::RunLoop run_loop;
  handler_ = CreateHandler();

  ON_CALL(*extension_wrapper_ptr_, RequestLanguageInstall)
      .WillByDefault(
          [&](const std::string& language, OnInstallCompleteCallback callback) {
            std::move(callback).Run(result);
          });
  EXPECT_CALL(page_, OnGetVoicePackInfo)
      .WillOnce([&](read_anything::mojom::VoicePackInfoPtr info) {
        EXPECT_EQ(kLang, info->language);
        EXPECT_EQ(read_anything::mojom::ErrorCode::kWrongId,
                  info->pack_state->get_error_code());
        run_loop.Quit();
      });

  InstallVoicePack(kLang);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       GetVoicePackInfo_RequestsAreQueued) {
  base::RunLoop run_loop;
  const char kLang1[] = "en-us";
  const char kLang2[] = "fr-fr";
  PackResult result1;
  result1.pack_state = PackResult::StatusCode::kInstalled;
  result1.operation_error = PackResult::ErrorCode::kNone;
  result1.language_code = kLang1;
  PackResult result2;
  result2.pack_state = PackResult::StatusCode::kNotInstalled;
  result2.operation_error = PackResult::ErrorCode::kNone;
  result2.language_code = kLang2;
  handler_ = CreateHandler();

  // Send two info requests. Only the first should be processed.
  GetPackStateCallback callback1;
  EXPECT_CALL(*extension_wrapper_ptr_, RequestLanguageInfo)
      .WillOnce(
          [&](const std::string& language, GetPackStateCallback callback) {
            EXPECT_EQ(language, kLang1);
            callback1 = std::move(callback);
          });
  GetVoicePackInfo(kLang1);
  GetVoicePackInfo(kLang2);

  // After we get the result from the first request, then send the second one.
  GetPackStateCallback callback2;
  EXPECT_CALL(*extension_wrapper_ptr_, RequestLanguageInfo)
      .WillOnce(
          [&](const std::string& language, GetPackStateCallback callback) {
            EXPECT_EQ(language, kLang2);
            callback2 = std::move(callback);
          });
  std::move(callback1).Run(result1);

  EXPECT_CALL(page_, OnGetVoicePackInfo)
      .WillOnce([&](read_anything::mojom::VoicePackInfoPtr info) {
        EXPECT_EQ(kLang1, info->language);
        EXPECT_EQ(InstallationState::kInstalled,
                  info->pack_state->get_installation_state());
      })
      .WillOnce([&](read_anything::mojom::VoicePackInfoPtr info) {
        EXPECT_EQ(kLang2, info->language);
        EXPECT_EQ(InstallationState::kNotInstalled,
                  info->pack_state->get_installation_state());
        run_loop.Quit();
      });
  std::move(callback2).Run(result2);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       InstallVoicePack_RequestsAreQueued) {
  base::RunLoop run_loop;
  const char kLang1[] = "en-us";
  const char kLang2[] = "fr-fr";
  PackResult result1;
  result1.pack_state = PackResult::StatusCode::kInstalled;
  result1.operation_error = PackResult::ErrorCode::kNone;
  result1.language_code = kLang1;
  PackResult result2;
  result2.pack_state = PackResult::StatusCode::kNotInstalled;
  result2.operation_error = PackResult::ErrorCode::kNone;
  result2.language_code = kLang2;
  handler_ = CreateHandler();

  // Send two requests. Only the first install request should be processed.
  OnInstallCompleteCallback installCallback;
  GetPackStateCallback infoCallback;
  EXPECT_CALL(*extension_wrapper_ptr_, RequestLanguageInstall)
      .WillOnce(
          [&](const std::string& language, OnInstallCompleteCallback callback) {
            EXPECT_EQ(language, kLang1);
            installCallback = std::move(callback);
          });
  InstallVoicePack(kLang1);
  InstallVoicePack(kLang2);

  // After getting the install callback, we first request info for that
  // language, and that request should go through right away.
  EXPECT_CALL(*extension_wrapper_ptr_, RequestLanguageInfo)
      .WillOnce(
          [&](const std::string& language, GetPackStateCallback callback) {
            EXPECT_EQ(language, kLang1);
            infoCallback = std::move(callback);
          });
  std::move(installCallback).Run(result1);

  // After getting the info callback, move to the next language in the queue.
  EXPECT_CALL(*extension_wrapper_ptr_, RequestLanguageInstall)
      .WillOnce(
          [&](const std::string& language, OnInstallCompleteCallback callback) {
            EXPECT_EQ(language, kLang2);
            installCallback = std::move(callback);
          });
  std::move(infoCallback).Run(result1);

  // After receiving the install callback for lang2, we should request the
  // status for that.
  EXPECT_CALL(*extension_wrapper_ptr_, RequestLanguageInfo)
      .WillOnce(
          [&](const std::string& language, GetPackStateCallback callback) {
            EXPECT_EQ(language, kLang2);
            infoCallback = std::move(callback);
          });
  std::move(installCallback).Run(result2);

  EXPECT_CALL(page_, OnGetVoicePackInfo)
      .WillOnce([&](read_anything::mojom::VoicePackInfoPtr info) {
        EXPECT_EQ(kLang1, info->language);
        EXPECT_EQ(InstallationState::kInstalled,
                  info->pack_state->get_installation_state());
      })
      .WillOnce([&](read_anything::mojom::VoicePackInfoPtr info) {
        EXPECT_EQ(kLang2, info->language);
        EXPECT_EQ(InstallationState::kNotInstalled,
                  info->pack_state->get_installation_state());
        run_loop.Quit();
      });
  std::move(infoCallback).Run(result2);

  run_loop.Run();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest, OnTabWillDetach) {
  handler_ = CreateHandler();

  OnTabWillDetach();
  EXPECT_CALL(page_, OnTabWillDetach).Times(1);
  EXPECT_CALL(page_, OnReadingModeHidden).Times(0);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnTabWillDetach_SendsOnce) {
  handler_ = CreateHandler();

  OnTabWillDetach();
  OnTabWillDetach();
  OnTabWillDetach();
  EXPECT_CALL(page_, OnTabWillDetach).Times(1);
  EXPECT_CALL(page_, OnReadingModeHidden).Times(0);
}
IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnTabWillDetach_ResetsAudio) {
  handler_ = CreateHandler();
  handler_->OnReadAloudAudioStateChange(true);

  OnTabWillDetach();

  EXPECT_TRUE(base::test::RunUntil([&]() { return !HasAudio(); }));
  ASSERT_FALSE(HasAudio());
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       Activate_OnCloseReadingMode_NotifiesPage) {
  handler_ = CreateHandler();
  Activate(false);
  EXPECT_CALL(page_, OnReadingModeHidden(true)).Times(1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       Activate_OnDeactivateTab_NotifiesPage) {
  handler_ = CreateHandler();
  ASSERT_TRUE(embedded_test_server()->Start());
  // Store the controller since it is per-tab, and a new tab will be activated
  // below.
  auto* original_controller = side_panel_controller();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(embedded_test_server()->GetURL("/simple.html")),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Indicate the original tab is now hidden.
  original_controller->OnEntryHidden(read_anything_entry());

  ASSERT_FALSE(original_controller->tab()->IsActivated());
  ASSERT_NE(original_controller, side_panel_controller());
  EXPECT_CALL(page_, OnReadingModeHidden(false)).Times(1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       Activate_OnActivateTab_DoesNotNotifyPage) {
  handler_ = CreateHandler();

  Activate(true);
  EXPECT_CALL(page_, OnReadingModeHidden).Times(0);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingUntrustedPageHandlerTest,
    OnDistillationStatus_AfterActivateWithOmnibox_LogsStatus) {
  base::HistogramTester histogram_tester;
  handler_ = CreateHandler();
  auto status = read_anything::mojom::DistillationStatus::kSuccess;
  int word_count = 3001;
  SidePanelOpenTrigger trigger = SidePanelOpenTrigger::kReadAnythingOmniboxChip;
  Activate(true, &trigger);

  handler_->OnDistillationStatus(status, word_count);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.DistillationStatusAfterOmnibox", status, 1);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.WordsDistilledAfterOmnibox", word_count, 1);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingUntrustedPageHandlerTest,
    OnDistillationStatus_AfterActivateWithOtherEntrypoint_DoesNotLogStatus) {
  base::HistogramTester histogram_tester;
  handler_ = CreateHandler();
  auto status = read_anything::mojom::DistillationStatus::kSuccess;
  int word_count = 3002;
  SidePanelOpenTrigger trigger = SidePanelOpenTrigger::kReadAnythingContextMenu;
  Activate(true, &trigger);

  handler_->OnDistillationStatus(status, word_count);

  histogram_tester.ExpectTotalCount(
      "Accessibility.ReadAnything.DistillationStatusAfterOmnibox", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.ReadAnything.WordsDistilledAfterOmnibox", 0);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingUntrustedPageHandlerTest,
    OnDistillationStatus_AfterAlreadyLogged_DoesNotLogStatusAgain) {
  base::HistogramTester histogram_tester;
  handler_ = CreateHandler();
  auto status1 = read_anything::mojom::DistillationStatus::kSuccess;
  auto status2 = read_anything::mojom::DistillationStatus::kFailure;
  int word_count1 = 3003;
  int word_count2 = 3004;
  SidePanelOpenTrigger trigger = SidePanelOpenTrigger::kReadAnythingOmniboxChip;
  Activate(true, &trigger);

  handler_->OnDistillationStatus(status1, word_count1);
  handler_->OnDistillationStatus(status2, word_count2);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.DistillationStatusAfterOmnibox", status1, 1);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.WordsDistilledAfterOmnibox", word_count1, 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnDistillationStatus_AfterDeactivate_StillLogsStatus) {
  base::HistogramTester histogram_tester;
  handler_ = CreateHandler();
  auto status = read_anything::mojom::DistillationStatus::kSuccess;
  int word_count = 3005;
  SidePanelOpenTrigger trigger = SidePanelOpenTrigger::kReadAnythingOmniboxChip;
  Activate(true, &trigger);

  Activate(false);
  handler_->OnDistillationStatus(status, word_count);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.DistillationStatusAfterOmnibox", status, 1);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.WordsDistilledAfterOmnibox", word_count, 1);
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingUntrustedPageHandlerTest,
    OnDistillationStatus_AfterDeactivateAndStatusAlreadyLogged_DoesNotLogStatus) {
  base::HistogramTester histogram_tester;
  handler_ = CreateHandler();
  auto status = read_anything::mojom::DistillationStatus::kSuccess;
  int word_count = 3006;
  SidePanelOpenTrigger trigger = SidePanelOpenTrigger::kReadAnythingOmniboxChip;
  Activate(true, &trigger);

  handler_->OnDistillationStatus(status, word_count);

  Activate(false);
  handler_->OnDistillationStatus(status, word_count);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.DistillationStatusAfterOmnibox", status, 1);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.WordsDistilledAfterOmnibox", word_count, 1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       DidUpdateAudioMutingState) {
  handler_ = CreateHandler();

  handler_->DidUpdateAudioMutingState(true);
  EXPECT_CALL(page_, OnTabMuteStateChange(true)).Times(1);
  handler_->DidUpdateAudioMutingState(false);
  EXPECT_CALL(page_, OnTabMuteStateChange(false)).Times(1);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingUntrustedPageHandlerTest,
                       OnReadAloudAudioStateChange) {
  handler_ = CreateHandler();

  ASSERT_FALSE(HasAudio());
  handler_->OnReadAloudAudioStateChange(true);
  ASSERT_TRUE(HasAudio());

  handler_->OnReadAloudAudioStateChange(false);

  EXPECT_TRUE(base::test::RunUntil([&]() { return !HasAudio(); }));
  ASSERT_FALSE(HasAudio());
}

}  // namespace

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/read_anything/read_anything.mojom-forward.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/language_detection/core/constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/tts_controller.h"
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

namespace {

using testing::_;
using testing::ElementsAre;

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
  MOCK_METHOD(void, OnReadingModeHidden, ());
  MOCK_METHOD(void, OnTabWillDetach, ());
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

class TestReadAnythingUntrustedPageHandler
    : public ReadAnythingUntrustedPageHandler {
 public:
  explicit TestReadAnythingUntrustedPageHandler(
      mojo::PendingRemote<read_anything::mojom::UntrustedPage> page,
      content::WebUI* test_web_ui)
      : ReadAnythingUntrustedPageHandler(
            std::move(page),
            mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandler>(),
            test_web_ui,
            /*use_screen_ai_service=*/false) {}

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
class ReadAnythingUntrustedPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    // `TestReadAnythingUntrustedPageHandler` disables ScreenAI service, which
    // disables using ReadAnythingWithScreen2x and PdfOcr.
    scoped_feature_list_.InitAndEnableFeature(
        {features::kReadAnythingReadAloud});
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL(url::kAboutBlankURL));
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
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

  void TearDown() override {
    handler_.reset();
    test_web_ui_.reset();
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  ReadAnythingSidePanelController* side_panel_controller() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->read_anything_side_panel_controller();
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

  void Activate(bool active) {
    SidePanelEntry* entry = browser()
                                ->GetActiveTabInterface()
                                ->GetTabFeatures()
                                ->side_panel_registry()
                                ->GetEntryForKey(SidePanelEntry::Key(
                                    SidePanelEntry::Id::kReadAnything));
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
  testing::NiceMock<MockPage> page_;
  FakeTtsEngineDelegate engine_delegate_;
  std::unique_ptr<ReadAnythingUntrustedPageHandler> handler_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAnythingUntrustedPageHandlerTest,
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
  PrefService* prefs = profile()->GetPrefs();
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

  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnLineSpaceChange) {
  const read_anything::mojom::LineSpacing kSpacing1 =
      read_anything::mojom::LineSpacing::kLoose;
  const read_anything::mojom::LineSpacing kSpacing2 =
      read_anything::mojom::LineSpacing::kStandard;
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnLineSpaceChange(kSpacing1);
  int spacing1 = profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingLineSpacing);
  ASSERT_EQ(spacing1, static_cast<int>(kSpacing1));

  OnLineSpaceChange(kSpacing2);
  int spacing2 = profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingLineSpacing);
  ASSERT_EQ(spacing2, static_cast<int>(kSpacing2));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnLetterSpaceChange) {
  const read_anything::mojom::LetterSpacing kSpacing1 =
      read_anything::mojom::LetterSpacing::kVeryWide;
  const read_anything::mojom::LetterSpacing kSpacing2 =
      read_anything::mojom::LetterSpacing::kStandard;
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnLetterSpaceChange(kSpacing1);
  const int spacing1 = profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingLetterSpacing);
  ASSERT_EQ(spacing1, static_cast<int>(kSpacing1));

  OnLetterSpaceChange(kSpacing2);
  const int spacing2 = profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingLetterSpacing);
  ASSERT_EQ(spacing2, static_cast<int>(kSpacing2));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnColorChange) {
  const read_anything::mojom::Colors kColor1 =
      read_anything::mojom::Colors::kBlue;
  const read_anything::mojom::Colors kColor2 =
      read_anything::mojom::Colors::kDark;
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnColorChange(kColor1);
  const int spacing1 = profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingColorInfo);
  ASSERT_EQ(spacing1, static_cast<int>(kColor1));

  OnColorChange(kColor2);
  const int spacing2 = profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingColorInfo);
  ASSERT_EQ(spacing2, static_cast<int>(kColor2));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnHighlightGranularityChanged) {
  const read_anything::mojom::HighlightGranularity kGranularity1 =
      read_anything::mojom::HighlightGranularity::kPhrase;
  const read_anything::mojom::HighlightGranularity kGranularity2 =
      read_anything::mojom::HighlightGranularity::kOff;
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnHighlightGranularityChanged(kGranularity1);
  const int granularity1 = profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingHighlightGranularity);
  ASSERT_EQ(granularity1, static_cast<int>(kGranularity1));

  OnHighlightGranularityChanged(kGranularity2);
  const int granularity2 = profile()->GetPrefs()->GetInteger(
      prefs::kAccessibilityReadAnythingHighlightGranularity);
  ASSERT_EQ(granularity2, static_cast<int>(kGranularity2));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnFontChange) {
  const char kFont1[] = "Atkinson Hyperlegible";
  const char kFont2[] = "Arial";
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnFontChange(kFont1);
  const std::string font1 = profile()->GetPrefs()->GetString(
      prefs::kAccessibilityReadAnythingFontName);
  ASSERT_EQ(font1, kFont1);

  OnFontChange(kFont2);
  const std::string font2 = profile()->GetPrefs()->GetString(
      prefs::kAccessibilityReadAnythingFontName);
  ASSERT_EQ(font2, kFont2);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnFontSizeChange) {
  const double kFontSize1 = 2;
  const double kFontSize2 = .5;
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnFontSizeChange(kFontSize1);
  const double fontSize1 = profile()->GetPrefs()->GetDouble(
      prefs::kAccessibilityReadAnythingFontScale);
  ASSERT_EQ(fontSize1, kFontSize1);

  OnFontSizeChange(kFontSize2);
  const double fontSize2 = profile()->GetPrefs()->GetDouble(
      prefs::kAccessibilityReadAnythingFontScale);
  ASSERT_EQ(fontSize2, kFontSize2);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnLinksEnabledChanged) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnLinksEnabledChanged(true);
  const double fontSize1 = profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityReadAnythingLinksEnabled);
  ASSERT_TRUE(fontSize1);

  OnLinksEnabledChanged(false);
  const double fontSize2 = profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityReadAnythingLinksEnabled);
  ASSERT_FALSE(fontSize2);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnImagesEnabledChanged) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnImagesEnabledChanged(true);
  const double fontSize1 = profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityReadAnythingImagesEnabled);
  ASSERT_TRUE(fontSize1);

  OnImagesEnabledChanged(false);
  const double fontSize2 = profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityReadAnythingImagesEnabled);
  ASSERT_FALSE(fontSize2);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnSpeechRateChange) {
  const double kRate1 = 1.5;
  const double kRate2 = .8;
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnSpeechRateChange(kRate1);
  const double rate1 = profile()->GetPrefs()->GetDouble(
      prefs::kAccessibilityReadAnythingSpeechRate);
  ASSERT_EQ(rate1, kRate1);

  OnSpeechRateChange(kRate2);
  const double rate2 = profile()->GetPrefs()->GetDouble(
      prefs::kAccessibilityReadAnythingSpeechRate);
  ASSERT_EQ(rate2, kRate2);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnLanguagePrefChange_StoresEnabledLangsInPrefs) {
  const char kLang1[] = "en-au";
  const char kLang2[] = "en-gb";
  const char kDisabledLang[] = "en-us";
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnLanguagePrefChange(kLang1, true);
  OnLanguagePrefChange(kLang2, true);
  OnLanguagePrefChange(kDisabledLang, false);

  const base::Value::List* langs = &profile()->GetPrefs()->GetList(
      prefs::kAccessibilityReadAnythingLanguagesEnabled);
  ASSERT_EQ(langs->size(), 2u);
  ASSERT_EQ((*langs)[0].GetString(), kLang1);
  ASSERT_EQ((*langs)[1].GetString(), kLang2);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnLanguagePrefChange_SameLang_StoresLatestInPrefs) {
  const char kLang[] = "bn";
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
  PrefService* prefs = profile()->GetPrefs();

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

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnLanguagePrefChange_SameLang_StoresOnce) {
  const char kLang[] = "bn";
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
  PrefService* prefs = profile()->GetPrefs();

  OnLanguagePrefChange(kLang, true);
  ASSERT_EQ(
      prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled).size(),
      1u);

  OnLanguagePrefChange(kLang, true);
  ASSERT_EQ(
      prefs->GetList(prefs::kAccessibilityReadAnythingLanguagesEnabled).size(),
      1u);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
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
  PrefService* prefs = profile()->GetPrefs();
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
      .WillOnce(testing::WithArgs<8, 9>(testing::Invoke(
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
          })));

  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnVoiceChange_StoresInPrefs) {
  const char kLang1[] = "hi";
  const char kLang2[] = "ja";
  const char kVoice1[] = "Ariel";
  const char kVoice2[] = "Sebastian";
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnVoiceChange(kVoice1, kLang1);
  OnVoiceChange(kVoice2, kLang2);

  const base::Value::Dict* voices = &profile()->GetPrefs()->GetDict(
      prefs::kAccessibilityReadAnythingVoiceName);
  ASSERT_EQ(voices->size(), 2u);
  EXPECT_THAT(
      *voices,
      base::test::DictionaryHasValues(
          base::Value::Dict().Set(kLang1, kVoice1).Set(kLang2, kVoice2)));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnVoiceChange_SameLang_StoresLatestInPrefs) {
  const char kLang[] = "es-es";
  const char kVoice1[] = "Simba";
  const char kVoice2[] = "Nala";
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnVoiceChange(kVoice1, kLang);
  OnVoiceChange(kVoice2, kLang);

  const base::Value::Dict* voices = &profile()->GetPrefs()->GetDict(
      prefs::kAccessibilityReadAnythingVoiceName);
  ASSERT_EQ(voices->size(), 1u);
  EXPECT_THAT(*voices,
              base::test::DictionaryHasValue(kLang, base::Value(kVoice2)));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnVoiceChange_SameVoiceDifferentLang_StoresBothInPrefs) {
  const char kLang1[] = "pt-pt";
  const char kLang2[] = "pt-br";
  const char kVoice[] = "Peter Parker";
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnVoiceChange(kVoice, kLang1);
  OnVoiceChange(kVoice, kLang2);

  const base::Value::Dict* voices = &profile()->GetPrefs()->GetDict(
      prefs::kAccessibilityReadAnythingVoiceName);
  ASSERT_EQ(voices->size(), 2u);
  EXPECT_THAT(*voices,
              base::test::DictionaryHasValues(
                  base::Value::Dict().Set(kLang1, kVoice).Set(kLang2, kVoice)));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, BadImageData) {
  auto test_handler_u_ptr =
      std::make_unique<TestReadAnythingUntrustedPageHandler>(
          page_.BindAndGetRemote(), test_web_ui_.get());
  auto* test_handler = test_handler_u_ptr.get();
  handler_ = std::move(test_handler_u_ptr);
  auto tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXNodeID node_id = 1;
  SkBitmap bitmap;
  test_handler->SetTestBitmap(bitmap);
  OnImageDataRequested(tree_id, node_id);
  EXPECT_CALL(page_, OnImageDataDownloaded(_, _, _)).Times(0);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnLanguageDetermined_SendsCodeToPage) {
  const char kLang1[] = "id-id";
  const char kLang2[] = "es-us";
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
  EXPECT_CALL(page_, SetLanguageCode("en")).Times(1);

  OnLanguageDetermined(kLang1);
  OnLanguageDetermined(kLang2);

  EXPECT_CALL(page_, SetLanguageCode(kLang1)).Times(1);
  EXPECT_CALL(page_, SetLanguageCode(kLang2)).Times(1);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnLanguageDetermined_SameCodeOnlySentOnce) {
  const char kLang1[] = "id-id";
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
  EXPECT_CALL(page_, SetLanguageCode("en")).Times(1);

  OnLanguageDetermined(kLang1);
  OnLanguageDetermined(kLang1);
  OnLanguageDetermined(kLang1);

  EXPECT_CALL(page_, SetLanguageCode(kLang1)).Times(1);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnLanguageDetermined_UnknownLanguageSendsEmpty) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
  EXPECT_CALL(page_, SetLanguageCode).Times(1);

  OnLanguageDetermined(language_detection::kUnknownLanguageCode);

  EXPECT_CALL(page_, SetLanguageCode("")).Times(1);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnLanguageDetermined_UnknownLanguageSendsEmptyEveryTime) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
  EXPECT_CALL(page_, SetLanguageCode).Times(1);

  OnLanguageDetermined(language_detection::kUnknownLanguageCode);
  OnLanguageDetermined(language_detection::kUnknownLanguageCode);
  OnLanguageDetermined(language_detection::kUnknownLanguageCode);

  EXPECT_CALL(page_, SetLanguageCode("")).Times(3);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, AccessibilityEventReceived) {
  ui::AXUpdatesAndEvents details;
  details.events = {};
  details.updates = {};
  details.ax_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  AccessibilityEventReceived(details);

  EXPECT_CALL(page_, AccessibilityEventReceived(details.ax_tree_id, _, _))
      .Times(1);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnActiveAXTreeIDChanged) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnActiveAXTreeIDChanged();

  // This is called once during construction, so we check for 2 calls here.
  EXPECT_CALL(page_, OnActiveAXTreeIDChanged).Times(2);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnActiveAXTreeIDChanged_SendsExistingLanguageCode) {
  const char kLang[] = "pt-br";
  SetTranslateSourceLanguage(kLang);

  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  // Sets the default language code.
  EXPECT_CALL(page_, SetLanguageCode).Times(1);
  // Sends the detected language code.
  EXPECT_CALL(page_, SetLanguageCode(kLang)).Times(1);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnActiveAXTreeIDChanged_SendsNewLanguageCode) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
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

TEST_F(
    ReadAnythingUntrustedPageHandlerTest,
    OnActiveAXTreeIDChanged_AfterTranslateDriverDestroyed_StillSendsLanguage) {
  const char kLang1[] = "pt-br";
  const char kLang2[] = "es-es";
  SetTranslateSourceLanguage(kLang1);
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());
  EXPECT_CALL(page_, SetLanguageCode).Times(1);
  EXPECT_CALL(page_, SetLanguageCode(kLang1)).Times(1);

  OnTranslateDriverDestroyed(GetChromeTranslateClient()->GetTranslateDriver());
  SetTranslateSourceLanguage(kLang2);
  OnActiveAXTreeIDChanged();

  EXPECT_CALL(page_, SetLanguageCode(kLang2)).Times(1);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ReadAnythingUntrustedPageHandlerTest, GetVoicePackInfo) {
  const char kLang1[] = "id-id";
  const char kLang2[] = "en-gb";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  GetVoicePackInfo(kLang1);
  ASSERT_EQ(kLang1, engine_delegate_.last_requested_status());

  GetVoicePackInfo(kLang2);
  ASSERT_EQ(kLang2, engine_delegate_.last_requested_status());
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, InstallVoicePack) {
  const char kLang1[] = "fr-fr";
  const char kLang2[] = "en-us";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  InstallVoicePack(kLang1);
  ASSERT_EQ(kLang1, engine_delegate_.last_requested_install());

  InstallVoicePack(kLang2);
  ASSERT_EQ(kLang2, engine_delegate_.last_requested_install());
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, UninstallVoice) {
  const char kLang1[] = "it-it";
  const char kLang2[] = "en-au";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  UninstallVoice(kLang1);
  ASSERT_EQ(kLang1, engine_delegate_.last_requested_uninstall());

  UninstallVoice(kLang2);
  ASSERT_EQ(kLang2, engine_delegate_.last_requested_uninstall());
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnUpdateLanguageStatus_NotInstalled) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      kLang, content::LanguageInstallStatus::NOT_INSTALLED, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(testing::WithArg<0>(
          testing::Invoke([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kNotInstalled,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          })));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       OnUpdateLanguageStatus_Installing) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      kLang, content::LanguageInstallStatus::INSTALLING, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(testing::WithArg<0>(
          testing::Invoke([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kInstalling,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          })));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnUpdateLanguageStatus_Installed) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      kLang, content::LanguageInstallStatus::INSTALLED, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(testing::WithArg<0>(
          testing::Invoke([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kInstalled,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          })));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnUpdateLanguageStatus_Failed) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      kLang, content::LanguageInstallStatus::FAILED, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(testing::WithArg<0>(
          testing::Invoke([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kUnknown,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          })));
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnUpdateLanguageStatus_Unknown) {
  const char kLang[] = "it-it";
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      &engine_delegate_);
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  content::TtsController::GetInstance()->UpdateLanguageStatus(
      kLang, content::LanguageInstallStatus::UNKNOWN, "");

  EXPECT_CALL(page_, OnGetVoicePackInfo(_))
      .WillOnce(testing::WithArg<0>(
          testing::Invoke([&](read_anything::mojom::VoicePackInfoPtr info) {
            EXPECT_EQ(read_anything::mojom::InstallationState::kUnknown,
                      info->pack_state->get_installation_state());
            EXPECT_EQ(kLang, info->language);
          })));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnTabWillDetach) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnTabWillDetach();
  EXPECT_CALL(page_, OnTabWillDetach).Times(1);
  EXPECT_CALL(page_, OnReadingModeHidden).Times(0);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest, OnTabWillDetach_SendsOnce) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  OnTabWillDetach();
  OnTabWillDetach();
  OnTabWillDetach();
  EXPECT_CALL(page_, OnTabWillDetach).Times(1);
  EXPECT_CALL(page_, OnReadingModeHidden).Times(0);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       Activate_OnDeactivateTab_NotifiesPage) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  Activate(false);
  EXPECT_CALL(page_, OnReadingModeHidden).Times(1);
}

TEST_F(ReadAnythingUntrustedPageHandlerTest,
       Activate_OnActivateTab_DoesNotNotifyPage) {
  handler_ = std::make_unique<TestReadAnythingUntrustedPageHandler>(
      page_.BindAndGetRemote(), test_web_ui_.get());

  Activate(true);
  EXPECT_CALL(page_, OnReadingModeHidden).Times(0);
}

}  // namespace

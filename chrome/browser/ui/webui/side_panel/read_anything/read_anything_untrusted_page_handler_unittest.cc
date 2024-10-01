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
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/prefs/pref_value_map.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/mojom/base/values.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/mojom/ax_event.mojom.h"
#include "ui/accessibility/mojom/ax_tree_id.mojom.h"
#include "ui/accessibility/mojom/ax_tree_update.mojom.h"
#include "ui/gfx/geometry/size.h"

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(void, OnDeviceLocked, ());
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
            test_web_ui) {}

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

// TODO: b/40927698 - Add more tests.
class ReadAnythingUntrustedPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingReadAloud},
        {features::kReadAnythingWithScreen2x, features::kPdfOcr});
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

  void OnVoiceChange(const std::string& voice, const std::string& lang) {
    handler_->OnVoiceChange(voice, lang);
  }

  void OnLanguagePrefChange(const std::string& lang, bool enabled) {
    handler_->OnLanguagePrefChange(lang, enabled);
  }

  void OnImageDataRequested(const ui::AXTreeID& target_tree_id,
                            ui::AXNodeID target_node_id) {
    handler_->OnImageDataRequested(target_tree_id, target_node_id);
  }

 protected:
  testing::NiceMock<MockPage> page_;
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
  double expected_speech_rate = kReadAnythingDefaultSpeechRate;
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
            EXPECT_THAT(voices, base::test::DictionaryHasValue(
                                    kLang1, base::Value(kVoice1)));
            EXPECT_THAT(voices, base::test::DictionaryHasValue(
                                    kLang2, base::Value(kVoice2)));
            EXPECT_THAT(voices, base::test::DictionaryHasValue(
                                    kLang3, base::Value(kVoice3)));
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
  EXPECT_THAT(*voices,
              base::test::DictionaryHasValue(kLang1, base::Value(kVoice1)));
  EXPECT_THAT(*voices,
              base::test::DictionaryHasValue(kLang2, base::Value(kVoice2)));
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
              base::test::DictionaryHasValue(kLang1, base::Value(kVoice)));
  EXPECT_THAT(*voices,
              base::test::DictionaryHasValue(kLang2, base::Value(kVoice)));
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

}  // namespace

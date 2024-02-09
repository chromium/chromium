// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/tts_utterance.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace quick_answers {

// Size constants.
inline constexpr int kContentHeaderWidth = 248;
inline constexpr int kContentTextWidth = 280;
inline constexpr int kSubContentViewIndent = 12;
inline constexpr int kSubContentTextWidth =
    kContentTextWidth - kSubContentViewIndent;

// Spacing constants.
inline constexpr int kContentSingleSpacing = 8;
inline constexpr int kContentDoubleSpacing = 16;
inline constexpr gfx::Insets kViewHorizontalSpacingMargins =
    gfx::Insets::TLBR(0, 0, 0, kContentSingleSpacing);

// View constants.
inline constexpr int kRichAnswersIconContainerRadius = 24;
inline constexpr int kRichAnswersIconSizeDip = 16;
inline constexpr int kRichAnswersIconBorderDip = 4;
inline constexpr gfx::Insets kSubContentViewInsets =
    gfx::Insets::TLBR(0, kSubContentViewIndent, 0, 0);

// Font constants.
inline constexpr char kGoogleSansFont[] = "Google Sans";
inline constexpr char kRobotoFont[] = "Roboto";

// TTS constants.
inline constexpr char kGoogleTtsEngineId[] = "com.google.android.tts";

// The lifetime of instances of this class is manually bound to the lifetime of
// the associated TtsUtterance. See OnTtsEvent.
class QuickAnswersUtteranceEventDelegate
    : public content::UtteranceEventDelegate {
 public:
  QuickAnswersUtteranceEventDelegate() = default;
  ~QuickAnswersUtteranceEventDelegate() override = default;

  // UtteranceEventDelegate methods:
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int char_length,
                  const std::string& error_message) override;
};

// |TypographyToken| values used by the Quick Answers cards.
enum class TypographyToken {
  kCrosBody2,
  kCrosBody2Italic,
  kCrosButton1,
  kCrosButton2,
  kCrosDisplay5,
  kCrosTitle1
};

// Returns the |FontList| equivalents of |TypographyToken| values.
// This is so Quick Answers doesn't have an //ash/style dependency.
gfx::FontList GetFontList(TypographyToken token);

// Return the icon that corresponds to the Quick Answers result type.
const gfx::VectorIcon& GetResultTypeIcon(ResultType result_type);

// Adds the list of |QuickAnswerUiElement| horizontally to the container.
// Returns the resulting container view.
views::View* AddHorizontalUiElements(
    views::View* container,
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& elements);

// Creates a vertical FlexLayoutView with |kViewSpacingMargins| spacing.
std::unique_ptr<views::BoxLayoutView> CreateVerticalBoxLayoutView();

// Creates a horizontal BoxLayoutView with |kContentSingleSpacing| spacing.
// These BoxLayoutViews are used as horizontal container views of the rich card
// `content_view_`.
std::unique_ptr<views::BoxLayoutView> CreateHorizontalBoxLayoutView();

// Creates a horizontal FlexLayoutView with |kContentSingleSpacing| spacing.
// These FlexLayoutViews are used as container views *within* a BoxLayout
// container view.
std::unique_ptr<views::FlexLayoutView> CreateHorizontalFlexLayoutView();

// Creates a separator view with |kContentSingleSpacing| vertical margins.
std::unique_ptr<views::Separator> CreateSeparatorView();

// Creates an image button view with the specified arguments.
std::unique_ptr<views::ImageButton> CreateImageButtonView(
    base::RepeatingClosure closure,
    ui::ImageModel image_model,
    ui::ColorId background_color,
    std::u16string tooltip_text);

// Return the GURL that will link to the google search result for the
// query text.
GURL GetDetailsUrlForQuery(const std::string& query);

void GenerateTTSAudio(content::BrowserContext* browser_context,
                      const std::string& text,
                      const std::string& locale);

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_UTIL_H_

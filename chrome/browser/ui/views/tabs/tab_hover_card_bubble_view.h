// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/metrics_util.h"
#endif

namespace gfx {
class ImageSkia;
class Rect;
class RenderText;
}

class Tab;

// Dialog that displays an informational hover card containing page information.
class TabHoverCardBubbleView : public views::BubbleDialogDelegateView {
 public:
  static constexpr base::TimeDelta kHoverCardSlideDuration =
      base::Milliseconds(200);

  // Helper class used to elide local filenames with a RenderText object that
  // is provided with the correct setup and formatting.
  class FilenameElider {
   public:
    using LineLengths = std::pair<size_t, size_t>;

    explicit FilenameElider(std::unique_ptr<gfx::RenderText> render_text);
    ~FilenameElider();

    // Returns the elided text. Equivalent to:
    //   Elide(GetLineLengths(display_rect))
    // See those methods for a detailed description.
    std::u16string Elide(const std::u16string& text,
                         const gfx::Rect& display_rect) const;

    // Returns the start of the image dimensions as typically formatted by
    // Chrome in page titles, as a hint at how to potentially elide or split
    // the title. Expects something in the format "title (width×height)".
    // Returns std::u16string::npos if this pattern isn't found, otherwise
    // returns the index of the opening parenthesis in the string.
    //
    // If the result isn't npos, then the character previous to the open paren
    // character is guaranteed to be whitespace.
    static std::u16string::size_type FindImageDimensions(
        const std::u16string& text);

   private:
    friend class TabHoverCardBubbleViewFilenameEliderTest;

    // Given the current text and a rectangle to display text in, returns the
    // maximum length in characters of the first and second lines.
    //
    // The first value is the number of characters from the beginning of the
    // text that will fit on the line. The second value is the number of
    // characters from the end of the text that will fit on a line, minus
    // enough space to insert an ellipsis.
    //
    // Note that the sum of the two values may be greater than the length of
    // the text. Both segments are guaranteed to end at grapheme boundaries.
    LineLengths GetLineLengths(const gfx::Rect& display_rect) const;

    // Returns a string formatted for two-line elision given the last string
    // passed to SetText() and the maximum extent of the first and second
    // lines. The resulting string will either be the original text (if it fits
    // on one line) or the first line, followed by a newline, an ellipsis, and
    // the second line. The cut points passed in must be at grapheme
    // boundaries.
    //
    // If the two lines overlap (that is, if the line lengths sum to more than
    // the length of the original text), an optimum breakpoint will be chosen
    // to insert the newline:
    //  * If possible, the extension (and if it's an image, the image
    //    dimensions) will be placed alone on the second line.
    //  * Otherwise, as many characters as possible will be placed on the first
    //    line.
    // TODO(dfried): consider optimizing to break at natural breaks: spaces,
    // punctuation, etc.
    //
    // Note that if the extension is isolated on the second line or an ellipsis
    // is inserted, the second line will be marked as a bidirectional isolate,
    // so that its direction is determined by the leading text on the line
    // rather than whatever is "left over" from the first line. We find this
    // produces a much more visually appealing and less confusing result than
    // inheriting the preceding directionality.
    std::u16string ElideImpl(LineLengths line_lengths) const;

    std::unique_ptr<gfx::RenderText> render_text_;
  };

  METADATA_HEADER(TabHoverCardBubbleView);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHoverCardBubbleElementId);
  explicit TabHoverCardBubbleView(Tab* tab);
  TabHoverCardBubbleView(const TabHoverCardBubbleView&) = delete;
  TabHoverCardBubbleView& operator=(const TabHoverCardBubbleView&) = delete;
  ~TabHoverCardBubbleView() override;

  // Updates and formats title, alert state, domain, and preview image.
  void UpdateCardContent(const Tab* tab);

  // Update the text fade to the given percent, which should be between 0 and 1.
  void SetTextFade(double percent);

  // Set the preview image to use for the target tab.
  void SetTargetTabImage(gfx::ImageSkia preview_image);

  // Specifies that the hover card should display a placeholder image
  // specifying that no preview for the tab is available (yet).
  void SetPlaceholderImage();

  // Accessors used by tests.
  std::u16string GetTitleTextForTesting() const;
  std::u16string GetDomainTextForTesting() const;

  // Returns the percentage complete during transition animations when a
  // pre-emptive crossfade to a placeholder should start if a new image is not
  // available, or `absl::nullopt` to disable crossfades entirely.
  static absl::optional<double> GetPreviewImageCrossfadeStart();

 private:
  class FadeLabel;
  class ThumbnailView;

  bool using_rounded_corners() const { return corner_radius_.has_value(); }

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  raw_ptr<FadeLabel> title_label_ = nullptr;
  raw_ptr<FadeLabel> domain_label_ = nullptr;
  raw_ptr<ThumbnailView> thumbnail_view_ = nullptr;
  absl::optional<TabAlertState> alert_state_;

  absl::optional<int> corner_radius_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

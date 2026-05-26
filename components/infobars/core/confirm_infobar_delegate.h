// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_CONFIRM_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_CONFIRM_INFOBAR_DELEGATE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/text_constants.h"

namespace infobars {
class InfoBar;
}

namespace ui {
class ImageModel;
}

// Represents a single substitution element for a localized template string.
// Used by ConfirmInfoBarDelegate to provide structured metadata about
// parts of the message string that require special styling (e.g., links)
// or custom accessibility handling.
struct MessageSubstitution {
  MessageSubstitution(std::u16string text,
                      bool is_link,
                      std::optional<std::u16string> accessible_name);
  MessageSubstitution(const MessageSubstitution& other);
  MessageSubstitution(MessageSubstitution&& other);
  MessageSubstitution& operator=(const MessageSubstitution& other);
  MessageSubstitution& operator=(MessageSubstitution&& other);
  ~MessageSubstitution();

  std::u16string text;
  bool is_link = false;
  std::optional<std::u16string> accessible_name;
};

// An interface derived from InfoBarDelegate implemented by objects wishing to
// control a ConfirmInfoBar.
class ConfirmInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  enum InfoBarButton {
    BUTTON_NONE = 0,
    BUTTON_OK = 1 << 0,
    BUTTON_CANCEL = 1 << 1,
  };

  ConfirmInfoBarDelegate(const ConfirmInfoBarDelegate&) = delete;
  ConfirmInfoBarDelegate& operator=(const ConfirmInfoBarDelegate&) = delete;
  ~ConfirmInfoBarDelegate() override;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAccept() {}
    virtual void OnDismiss() {}
  };

  // InfoBarDelegate:
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
  void InfoBarDismissed() override;
  const ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate() const override;

  // Returns the title string to be displayed for the InfoBar.
  // Defaults to having not title. Currently only used on iOS.
  virtual std::u16string GetTitleText() const;

  // Returns the message string to be displayed for the InfoBar.
  virtual std::u16string GetMessageText() const = 0;

  // Returns the localized template string to be displayed for the InfoBar.
  // This string can contain placeholders (e.g. "$1", "$2") that will be
  // replaced by the strings returned by GetMessageSubstitutions().
  // If this returns an empty string, GetMessageText() is used instead.
  virtual std::u16string GetMessageTextTemplate() const;

  // Returns the list of substitutions to be used with the template
  // returned by GetMessageTextTemplate().
  virtual std::vector<MessageSubstitution> GetMessageSubstitutions() const;

  // Returns the elide behavior for the message string.
  // Not supported on Android.
  virtual gfx::ElideBehavior GetMessageElideBehavior() const;

  // Returns the buttons to be shown for this InfoBar.
  virtual int GetButtons() const;

  // Returns the label for the specified button. The default implementation
  // returns "OK" for the OK button and "Cancel" for the Cancel button.
  virtual std::u16string GetButtonLabel(InfoBarButton button) const;

  // Returns the label for the specified button. The default implementation
  // returns an empty image.
  virtual ui::ImageModel GetButtonImage(InfoBarButton button) const;

  // Returns whether the specified button is enabled. The default implementation
  // returns true.
  virtual bool GetButtonEnabled(InfoBarButton button) const;

  // Returns the tooltip for the specified button. The default implementation
  // returns an empty tooltip.
  virtual std::u16string GetButtonTooltip(InfoBarButton button) const;

  // Returns the style for the specified button. The default implementation
  // returns std::nullopt, which means default styling logic will be used.
  virtual std::optional<ui::ButtonStyle> GetButtonStyle(
      InfoBarButton button) const;

  // Returns true if this specific infobar instance should use the
  // custom layout to show the link text before the button.
  virtual bool ShouldShowLinkBeforeButton() const;

  // Returns spacing which is to be used when the link shows before the button
  // on the infobar.
  virtual int GetLinkSpacingWhenPositionedBeforeButton() const;

#if BUILDFLAG(IS_IOS)
  // Returns whether or not a tint should be applied to the icon background.
  // Defaults to true.
  virtual bool UseIconBackgroundTint() const;

  // Returns whether or not the icon image colors should be ignored when the
  // background tint is applied. Defaults to true (which forces template
  // rendering mode).
  virtual bool IgnoreIconColorWithTint() const;
#endif

  // Called when the OK button is pressed. If this function returns true, the
  // infobar is then immediately closed. Subclasses MUST NOT return true if in
  // handling this call something triggers the infobar to begin closing.
  virtual bool Accept();

  // Called when the Cancel button is pressed. If this function returns true,
  // the infobar is then immediately closed. Subclasses MUST NOT return true if
  // in handling this call something triggers the infobar to begin closing.
  virtual bool Cancel();

  // Called when an inline link created via the substitution system is clicked.
  // The |disposition| specifies how the resulting document should be loaded
  // (based on the event flags present when the link was clicked).
  // If this function returns true, the infobar is then immediately closed.
  virtual bool InlineSubstitutionLinkClicked(size_t index,
                                             WindowOpenDisposition disposition);

  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

 protected:
  ConfirmInfoBarDelegate();

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_INFOBARS_CORE_CONFIRM_INFOBAR_DELEGATE_H_

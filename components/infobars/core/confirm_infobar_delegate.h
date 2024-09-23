// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_CONFIRM_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_CONFIRM_INFOBAR_DELEGATE_H_

#include <string>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_constants.h"

namespace infobars {
class InfoBar;
}

namespace ui {
class ImageModel;
}

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
  ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate() override;

  // Returns the title string to be displayed for the InfoBar.
  // Defaults to having not title. Currently only used on iOS.
  virtual std::u16string GetTitleText() const;

  // Returns the message string to be displayed for the InfoBar.
  virtual std::u16string GetMessageText() const = 0;

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

#if BUILDFLAG(IS_IOS)
  // Returns whether or not a tint should be applied to the icon background.
  // Defaults to true.
  virtual bool UseIconBackgroundTint() const;
#endif

  // Called when the OK button is pressed. If this function returns true, the
  // infobar is then immediately closed. Subclasses MUST NOT return true if in
  // handling this call something triggers the infobar to begin closing.
  virtual bool Accept();

  // Called when the Cancel button is pressed. If this function returns true,
  // the infobar is then immediately closed. Subclasses MUST NOT return true if
  // in handling this call something triggers the infobar to begin closing.
  virtual bool Cancel();

  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

 protected:
  ConfirmInfoBarDelegate();

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // COMPONENTS_INFOBARS_CORE_CONFIRM_INFOBAR_DELEGATE_H_

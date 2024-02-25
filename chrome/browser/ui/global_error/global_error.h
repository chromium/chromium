// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_H_
#define CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/models/image_model.h"

class Browser;
class GlobalErrorBubbleViewBase;

// This object describes a single global error.
class GlobalError {
 public:
  enum Severity {
    SEVERITY_LOW,
    SEVERITY_MEDIUM,
    SEVERITY_HIGH,
  };

  GlobalError();
  virtual ~GlobalError();

  // Returns the error's severity level. If there are multiple errors,
  // the error with the highest severity will display in the menu. If not
  // overridden, this is based on the badge resource ID.
  virtual Severity GetSeverity();

  // Returns true if a menu item should be added to the app menu.
  virtual bool HasMenuItem() = 0;
  // Returns the command ID for the menu item.
  virtual int MenuItemCommandID() = 0;
  // Returns the label for the menu item.
  virtual std::u16string MenuItemLabel() = 0;
  // Returns the menu item icon.
  virtual ui::ImageModel MenuItemIcon();
  // Called when the user clicks on the menu item.
  virtual void ExecuteMenuItem(Browser* browser) = 0;

  // Returns true if a bubble view should be shown.
  virtual bool HasBubbleView() = 0;
  // Returns true if the bubble view has been shown.
  virtual bool HasShownBubbleView() = 0;
  // Called to show the bubble view.
  virtual void ShowBubbleView(Browser* browser) = 0;
  // Returns the bubble view.
  virtual GlobalErrorBubbleViewBase* GetBubbleView() = 0;
};

// This object describes a single global error that already comes with support
// for showing a standard Bubble UI. Derived classes just need to supply the
// content to be displayed in the bubble.
class GlobalErrorWithStandardBubble : public GlobalError {
 public:
  GlobalErrorWithStandardBubble();

  GlobalErrorWithStandardBubble(const GlobalErrorWithStandardBubble&) = delete;
  GlobalErrorWithStandardBubble& operator=(
      const GlobalErrorWithStandardBubble&) = delete;

  ~GlobalErrorWithStandardBubble() override;

  // Override these methods to customize the contents of the error bubble:
  virtual std::u16string GetBubbleViewTitle() = 0;
  virtual std::vector<std::u16string> GetBubbleViewMessages() = 0;
  virtual std::u16string GetBubbleViewAcceptButtonLabel() = 0;
  virtual bool ShouldShowCloseButton() const;
  virtual bool ShouldAddElevationIconToAcceptButton();
  virtual std::u16string GetBubbleViewCancelButtonLabel() = 0;
  virtual bool ShouldCloseOnDeactivate() const;
  virtual std::u16string GetBubbleViewDetailsButtonLabel();

  // Override these methods to be notified when events happen on the bubble:
  virtual void OnBubbleViewDidClose(Browser* browser) = 0;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) = 0;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) = 0;
  virtual void BubbleViewDetailsButtonPressed(Browser* browser);

  // Leaf classes must provide a WeakPtr to themselves.
  virtual base::WeakPtr<GlobalErrorWithStandardBubble> AsWeakPtr() = 0;

  // GlobalError overrides:
  bool HasBubbleView() override;
  bool HasShownBubbleView() override;
  void ShowBubbleView(Browser* browser) override;
  GlobalErrorBubbleViewBase* GetBubbleView() override;

  // This method is used by the View to notify this object that the bubble has
  // closed. Do not call it. It is only virtual for unit tests; do not override
  // it either.
  virtual void BubbleViewDidClose(Browser* browser);

 private:
  bool has_shown_bubble_view_ = false;
  raw_ptr<GlobalErrorBubbleViewBase> bubble_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_H_

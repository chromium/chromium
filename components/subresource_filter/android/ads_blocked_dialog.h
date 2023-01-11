// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_ANDROID_ADS_BLOCKED_DIALOG_H_
#define COMPONENTS_SUBRESOURCE_FILTER_ANDROID_ADS_BLOCKED_DIALOG_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"

namespace content {
class WebContents;
}  // namespace content

// The dialog class to invoke ads blocked dialog in Java from native ads blocked
// delegate code.
// The native code owns the lifetime of the dialog.
// AdsBlockedDialog::Create() creates an instance of the dialog. It can
// return nullptr when WebContents is not attached to any window. Show()
// displays the dialog on the screen.
//
// OnAllowAdsClicked callback is called when the user allows ads for the
// site in question. OnLearnMoreClicked callback is called when the user clicks
// the 'Learn more' button.
//
// The dialog will be dismissed after both callbacks, feature
// code shouldn't call Dismiss from callback implementation to dismiss the
// dialog. The implementation of both callbacks should destroy the dialog
// dialog instance.
//
// Here is how typically dialog is created:
//   m_dialog = AdsBlockedDialog::Create(web_contents,
//      base::BindOnce(&OnAllowAdsClicked),base::BindOnce(&OnLearnMoreClicked));
//   if (m_dialog) m_dialog->Show(...);
//
// The owning class should dismiss displayed dialog during its own destruction:
//   if (m_dialog) m_dialog->Dismiss();
//
// Dialog instance should be destroyed in both callbacks:
//   {
//     m_dialog.reset();
//   }
//
// The AdsBlockedDialogBase is the interface created to facilitate mocking in
// tests. AdsBlockedDialog contains the implementation.
class AdsBlockedDialogBase {
 public:
  virtual ~AdsBlockedDialogBase();

  // Calls Java side of the dialog to display ads blocked modal dialog.
  virtual void Show(bool should_post_dialog) = 0;

  // Dismisses displayed dialog. The owner of AdsBlockedDialog should
  // call this function to correctly dismiss and destroy the dialog. The object
  // can be safely destroyed after both callbacks are executed.
  virtual void Dismiss() = 0;
};

class AdsBlockedDialog : public AdsBlockedDialogBase {
 public:
  ~AdsBlockedDialog() override;

  // Creates and returns an instance of AdsBlockedDialog and
  // corresponding Java counterpart.
  // Returns nullptr if |web_contents| is not attached to a window.
  static std::unique_ptr<AdsBlockedDialogBase> Create(
      content::WebContents* web_contents,
      base::OnceClosure allow_ads_clicked_callback,
      base::OnceClosure learn_more_clicked_callback,
      base::OnceClosure dialog_dismissed_callback);

  // Disallow copy and assign.
  AdsBlockedDialog(const AdsBlockedDialog&) = delete;
  AdsBlockedDialog& operator=(const AdsBlockedDialog&) = delete;

  // Calls Java side of the dialog to display ads blocked modal dialog.
  void Show(bool should_post_dialog) override;

  // Dismisses displayed dialog. The owner of AdsBlockedDialog should
  // call this function to correctly dismiss and destroy the dialog. The object
  // can be safely destroyed after both callbacks are executed.
  void Dismiss() override;

  // Called from Java to indicate that the user tapped the positive button to
  // allow ads for the site in question.
  void OnAllowAdsClicked(JNIEnv* env);

  // Called from Java when the user clicks on 'Learn more'.
  void OnLearnMoreClicked(JNIEnv* env);

  // Called from Java when the dialog is dismissed.
  void OnDismissed(JNIEnv* env);

 private:
  AdsBlockedDialog(base::android::ScopedJavaLocalRef<jobject> jwindow_android,
                   base::OnceClosure allow_ads_clicked_callback,
                   base::OnceClosure learn_more_clicked_callback,
                   base::OnceClosure dialog_dismissed_callback);

  base::android::ScopedJavaGlobalRef<jobject> java_ads_blocked_dialog_;
  base::OnceClosure allow_ads_clicked_callback_;
  base::OnceClosure learn_more_clicked_callback_;
  base::OnceClosure dialog_dismissed_callback_;
};

#endif  // COMPONENTS_SUBRESOURCE_FILTER_ANDROID_ADS_BLOCKED_DIALOG_H_

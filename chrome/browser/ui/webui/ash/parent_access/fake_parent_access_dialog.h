// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_FAKE_PARENT_ACCESS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_FAKE_PARENT_ACCESS_DIALOG_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"

namespace ash {

class FakeParentAccessDialogProvider : public ParentAccessDialogProvider {
 public:
  // Represents the action in the next Show() invocation.
  class Action {
   public:
    // Returns an action to invoke the `callback` passed to Show()
    // with the given `result` synchronously.
    static Action WithResult(
        std::unique_ptr<ParentAccessDialog::Result> result);

    // Returns an action to capture the `callback` passed to Show().
    // Example to capture the callback in tests:
    //   base::test::TestFuture<ParentAccessDialog::Callback> callback;
    //   fake_provider->SetNextAction(
    //       Action::CaptureCallback(callback.GetCallback()));
    //   ... // invoke provider's Show().
    //   ... = callback.Take();
    // Example to ignore the callback (so stuck at Show() infinitely):
    //   fake_provider->SetNextAction(
    //       Action::CaptureCallback(base::DoNothing()));
    static Action CaptureCallback(
        base::OnceCallback<void(ParentAccessDialog::Callback)> callback);

    // Returns an action to return kNotAChildUser error from Show().
    static Action NotAChildUser();

    // Returns an action to return kDialogAlreadyVisible error from Show().
    static Action DialogAlreadyVisible();

    Action(Action&& other);
    Action& operator=(Action&& other);
    ~Action();

   private:
    friend class FakeParentAccessDialogProvider;

    enum class Type {
      kWithResult,
      kCaptureCallback,
      kNotAChildUser,
      kDialogAlreadyVisible,
    };

    explicit Action(Type type);
    Type type_;
    std::unique_ptr<ParentAccessDialog::Result> next_result_;
    base::OnceCallback<void(ParentAccessDialog::Callback)> callback_;
  };

  FakeParentAccessDialogProvider();
  FakeParentAccessDialogProvider(const FakeParentAccessDialogProvider&) =
      delete;
  FakeParentAccessDialogProvider& operator=(
      const FakeParentAccessDialogProvider&) = delete;
  ~FakeParentAccessDialogProvider() override;

  // ParentAccessDialogProvider override:
  ShowError Show(parent_access_ui::mojom::ParentAccessParamsPtr params,
                 ParentAccessDialog::Callback callback) override;

  // Sets the next return action for Show().
  // See Action class's comments above for details.
  // This must be called before Show().
  void SetNextAction(Action next_action);

  // Returns the `params` passed to the last Show() call.
  parent_access_ui::mojom::ParentAccessParamsPtr TakeLastParams();

 private:
  std::optional<Action> next_action_;
  parent_access_ui::mojom::ParentAccessParamsPtr last_params_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_FAKE_PARENT_ACCESS_DIALOG_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_CALLBACK_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_CALLBACK_UTILS_H_

#include "base/functional/callback.h"

namespace web_app {

// RunChainedCallbacks() runs multiple callbacks chained together by
// successively binding the final callback as parameter to the one before it
// until the entire sequence has been bound together.
//
//
// Example usage:
//
// class ImageAlterationManager {
//  public:
//   void PromptUserToAlterImage(base::FilePath image_path,
//                               double alter_amount,
//                               base::OnceClosure callback) {
//     auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
//     RunChainedCallbacks(
//         base::BindOnce(&ImageAlterationManager::LoadImage, weak_ptr,
//                        image_path),
//         base::BindOnce(&ImageAlterationManager::AlterImage, weak_ptr,
//                        alter_amount),
//         base::BindOnce(&ImageAlterationManager::ConfirmWithUser, weak_ptr),
//         base::BindOnce(&ImageAlterationManager::MaybeWriteImage, weak_ptr,
//                        image_path),
//         std::move(callback));
//   }
//
//  private:
//   void LoadImage(base::FilePath image_path,
//                  base::OnceCallback<void(SkBitmap)> callback);
//   void AlterImage(base::OnceCallback<void(SkBitmap)> callback,
//                   double alter_amount,
//                   SkBitmap image);
//   void ConfirmWithUser(base::OnceCallback<void(SkBitmap, bool)> callback,
//                        SkBitmap image);
//   void MaybeWriteImage(base::FilePath image_path,
//                        base::OnceClosure callback,
//                        SkBitmap image,
//                        bool user_confirmed);
//
//   base::WeakPtrFactory<ImageAlterationManager> weak_ptr_factory_{this};
// };
//
//
// The alternate way to write PromptUserToAlterImage() without
// RunChainedCallbacks would be:
//
// base::BindOnce(
//     &ImageAlterationManager::LoadImage, weak_ptr, image_path,
//     base::BindOnce(
//         &ImageAlterationManager::AlterImage, weak_ptr, alter_amount,
//         base::BindOnce(
//             &ImageAlterationManager::ConfirmImageWithUser, weak_ptr,
//             base::BindOnce(&ImageAlterationManager::MaybeWriteImage,
//                            weak_ptr, image_path, std::move(callback)))));
//
// RunChainedCallbacks() avoids messy indented nesting of multiple
// base::BindOnce()s.

template <typename Callback>
Callback ChainCallbacks(Callback&& callback) {
  return std::forward<Callback>(callback);
}

template <typename FirstCallback, typename... NextCallbacks>
decltype(auto) ChainCallbacks(FirstCallback&& first_callback,
                              NextCallbacks&&... next_callbacks) {
  return base::BindOnce(std::forward<FirstCallback>(first_callback),
                        ChainCallbacks<NextCallbacks...>(
                            std::forward<NextCallbacks>(next_callbacks)...));
}

template <typename... Callbacks>
decltype(auto) RunChainedCallbacks(Callbacks&&... callbacks) {
  return ChainCallbacks(std::forward<Callbacks>(callbacks)...).Run();
}

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_CALLBACK_UTILS_H_

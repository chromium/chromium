// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_app_window_icon_observer.h"

#include <utility>

#include "base/hash/md5.h"
#include "base/run_loop.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

TestAppWindowIconObserver::TestAppWindowIconObserver(
    content::BrowserContext* context)
    : context_(context) {
  extensions::AppWindowRegistry::Get(context_)->AddObserver(this);
}

TestAppWindowIconObserver::~TestAppWindowIconObserver() {
  extensions::AppWindowRegistry::Get(context_)->RemoveObserver(this);
  for (aura::Window* window : windows_)
    window->RemoveObserver(this);
}

void TestAppWindowIconObserver::WaitForIconUpdate() {
  WaitForIconUpdates(1);
}

void TestAppWindowIconObserver::WaitForIconUpdates(int updates) {
  base::RunLoop run_loop;
  expected_icon_updates_ = updates + icon_updates_;
  icon_updated_callback_ = run_loop.QuitClosure();
  run_loop.Run();
  base::RunLoop().RunUntilIdle();
  DCHECK_EQ(expected_icon_updates_, icon_updates_);
}

void TestAppWindowIconObserver::OnAppWindowAdded(
    extensions::AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  window->AddObserver(this);
  windows_.push_back(window);
  // Handle the case where the app icon is already set prior to this callback.
  // If it is set, handle the icon update immediately.
  OnWindowPropertyChanged(window, aura::client::kAppIconKey, 0);
}

void TestAppWindowIconObserver::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  if (window) {
    windows_.erase(std::find(windows_.begin(), windows_.end(), window));
    window->RemoveObserver(this);
  }
}

void TestAppWindowIconObserver::OnWindowPropertyChanged(aura::Window* window,
                                                        const void* key,
                                                        intptr_t old) {
  if (key != aura::client::kAppIconKey)
    return;

  std::string app_icon_hash;
  const gfx::ImageSkia* image = window->GetProperty(aura::client::kAppIconKey);
  last_app_icon_ = image ? *image : gfx::ImageSkia();

  if (image && !image->isNull()) {
    const SkBitmap* bitmap = image->bitmap();

    // The window icon property changes more frequently than the image itself
    // leading to test flakiness. Just record instances where the image actually
    // changes.
    base::MD5Context ctx;
    base::MD5Init(&ctx);
    const size_t row_width = bitmap->bytesPerPixel() * bitmap->width();
    for (int y = 0; y < bitmap->height(); ++y) {
      base::MD5Update(
          &ctx,
          base::StringPiece(
              reinterpret_cast<const char*>(bitmap->getAddr(0, y)), row_width));
    }
    base::MD5Digest digest;
    base::MD5Final(&digest, &ctx);
    app_icon_hash = base::MD5DigestToBase16(digest);
  }

  if (app_icon_hash == last_app_icon_hash_map_[window])
    return;

  last_app_icon_hash_map_[window] = app_icon_hash;
  ++icon_updates_;
  if (icon_updates_ == expected_icon_updates_ &&
      !icon_updated_callback_.is_null()) {
    std::move(icon_updated_callback_).Run();
  }
}

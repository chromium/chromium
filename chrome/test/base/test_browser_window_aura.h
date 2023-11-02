// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_BROWSER_WINDOW_AURA_H_
#define CHROME_TEST_BASE_TEST_BROWSER_WINDOW_AURA_H_

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/test_browser_window.h"

#include <memory>

namespace aura {
class Window;
}
namespace views {
class Widget;
}

// A browser window proxy with an associated Aura native window.
// TODO: replace this with TestBrowserWindowViews.
class TestBrowserWindowAura : public TestBrowserWindow {
 public:
  explicit TestBrowserWindowAura(std::unique_ptr<aura::Window> native_window);
  TestBrowserWindowAura(const TestBrowserWindowAura&) = delete;
  TestBrowserWindowAura& operator=(const TestBrowserWindowAura&) = delete;
  ~TestBrowserWindowAura() override;

  // TestBrowserWindow overrides:
  gfx::NativeWindow GetNativeWindow() const override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  void Activate() override;
  bool IsActive() const override;
  gfx::Rect GetBounds() const override;

  std::unique_ptr<Browser> CreateBrowser(Browser::CreateParams* params);

 private:
  raw_ptr<Browser> browser_;  // not owned
  std::unique_ptr<aura::Window> native_window_;
};

class TestBrowserWindowViews : public TestBrowserWindow {
 public:
  explicit TestBrowserWindowViews(aura::Window* parent = nullptr);
  TestBrowserWindowViews(const TestBrowserWindowViews&) = delete;
  TestBrowserWindowViews& operator=(const TestBrowserWindowViews&) = delete;
  ~TestBrowserWindowViews() override;

  // TestBrowserWindow overrides:
  gfx::NativeWindow GetNativeWindow() const override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  void Activate() override;
  bool IsActive() const override;
  gfx::Rect GetBounds() const override;

  std::unique_ptr<Browser> CreateBrowser(const Browser::CreateParams& params);

 private:
  raw_ptr<Browser> browser_;  // not owned
  std::unique_ptr<views::Widget> widget_;
};

namespace chrome {

// Helper that creates a browser with a native Aura |window|. If |window| is
// nullptr, it will create an Aura window to associate with the browser. It also
// handles the lifetime of TestBrowserWindowAura.
std::unique_ptr<Browser> CreateBrowserWithAuraTestWindowForParams(
    std::unique_ptr<aura::Window> window,
    Browser::CreateParams* params);

// Helper that creates a browser with a Widget serving as the BrowserWindow.
std::unique_ptr<Browser> CreateBrowserWithViewsTestWindowForParams(
    const Browser::CreateParams& params,
    aura::Window* parent = nullptr);

}  // namespace chrome

#endif  // CHROME_TEST_BASE_TEST_BROWSER_WINDOW_AURA_H_

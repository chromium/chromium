// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_browser_window_aura.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace chrome {

std::unique_ptr<Browser> CreateBrowserWithAuraTestWindowForParams(
    std::unique_ptr<aura::Window> window,
    Browser::CreateParams* params) {
  if (!window) {
    window = std::make_unique<aura::Window>(nullptr);
    window->SetId(0);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->Show();
  }
  auto browser_window =
      std::make_unique<TestBrowserWindowAura>(std::move(window));
  auto browser = browser_window->CreateBrowser(params);
  // Self deleting.
  new TestBrowserWindowOwner(std::move(browser_window));
  return browser;
}

std::unique_ptr<Browser> CreateBrowserWithViewsTestWindowForParams(
    const Browser::CreateParams& params,
    aura::Window* parent) {
  auto browser_window = std::make_unique<TestBrowserWindowViews>(parent);
  auto browser = browser_window->CreateBrowser(params);
  // Self deleting.
  new TestBrowserWindowOwner(std::move(browser_window));
  return browser;
}

}  // namespace chrome

TestBrowserWindowAura::TestBrowserWindowAura(
    std::unique_ptr<aura::Window> native_window)
    : native_window_(std::move(native_window)) {}

TestBrowserWindowAura::~TestBrowserWindowAura() {}

gfx::NativeWindow TestBrowserWindowAura::GetNativeWindow() const {
  return native_window_.get();
}

void TestBrowserWindowAura::Show() {
  native_window_->Show();
}

void TestBrowserWindowAura::Hide() {
  native_window_->Hide();
}

bool TestBrowserWindowAura::IsVisible() const {
  return native_window_->IsVisible();
}

void TestBrowserWindowAura::Activate() {
  CHECK(native_window_->GetRootWindow())
      << "A TestBrowserWindowAura must have a root window to be activated.";
  ::wm::GetActivationClient(native_window_->GetRootWindow())
      ->ActivateWindow(native_window_.get());
}

bool TestBrowserWindowAura::IsActive() const {
  // A test window might not be parented.
  if (!native_window_->GetRootWindow())
    return false;
  return ::wm::GetActivationClient(native_window_->GetRootWindow())
             ->GetActiveWindow() == native_window_.get();
}

gfx::Rect TestBrowserWindowAura::GetBounds() const {
  return native_window_->bounds();
}

std::unique_ptr<Browser> TestBrowserWindowAura::CreateBrowser(
    Browser::CreateParams* params) {
  params->window = this;
  browser_ = Browser::Create(*params);
  return base::WrapUnique(browser_.get());
}

TestBrowserWindowViews::TestBrowserWindowViews(aura::Window* parent)
    : widget_(std::make_unique<views::Widget>()) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(5, 5, 20, 20);
  params.parent = parent;
  widget_->Init(std::move(params));
}

TestBrowserWindowViews::~TestBrowserWindowViews() {}

gfx::NativeWindow TestBrowserWindowViews::GetNativeWindow() const {
  return widget_->GetNativeWindow();
}

void TestBrowserWindowViews::Show() {
  widget_->Show();
}

void TestBrowserWindowViews::Hide() {
  widget_->Hide();
}

bool TestBrowserWindowViews::IsVisible() const {
  return widget_->IsVisible();
}

void TestBrowserWindowViews::Activate() {
  widget_->Activate();
}

bool TestBrowserWindowViews::IsActive() const {
  return widget_->IsActive();
}

gfx::Rect TestBrowserWindowViews::GetBounds() const {
  return widget_->GetWindowBoundsInScreen();
}

std::unique_ptr<Browser> TestBrowserWindowViews::CreateBrowser(
    const Browser::CreateParams& params) {
  Browser::CreateParams params_copy = params;
  params_copy.window = this;
  std::unique_ptr<Browser> browser(Browser::Create(params_copy));
  browser_ = browser.get();
  return browser;
}

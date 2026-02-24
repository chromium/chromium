// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_WEBBROWSER_H_
#define CHROME_UPDATER_WIN_UI_WEBBROWSER_H_

#include <objbase.h>

#include <ole2.h>
#include <windows.h>

#include <exdisp.h>
#include <mshtml.h>
#include <wrl/client.h>

#include <string>

namespace updater::ui {

// Wraps the MSHTML WebBrowser control.
class WebBrowser {
 public:
  WebBrowser();
  ~WebBrowser();

  WebBrowser(const WebBrowser&) = delete;
  WebBrowser& operator=(const WebBrowser&) = delete;

  int Initialize(HWND parent);
  void Show();

  void Navigate(const std::string& url);

  void Eval(const std::string& js);

  void SetTitle(const std::string& title);

  void SetSize(int width, int height, int hints);

  HWND hwnd() const { return hwnd_; }

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  bool CreateBrowser();

  HWND hwnd_ = nullptr;
  Microsoft::WRL::ComPtr<IWebBrowser2> web_browser_;
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_WEBBROWSER_H_

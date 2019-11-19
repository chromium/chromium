// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INPUT_METHOD_IME_WINDOW_H_
#define CHROME_BROWSER_UI_INPUT_METHOD_IME_WINDOW_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_icon_image.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class GURL;
class Profile;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}

namespace gfx {
class Rect;
}

namespace ui {

class ImeNativeWindow;
class ImeWindowObserver;

// The implementation for the IME window.
// This is used in the IME extension API.
// The IME window is in-activatable & always-on-top.
// There are 2 types of IME window:
//  - Normal type: the titlebar is located at top.
//  - Follow-cursor type: the titlebar is located at left, and the window is
//    auto-aligned with the text cursor or composition.
// The destructor is private, so the client should call Close() to release
// the instance. And Close() is async.
class ImeWindow : public content::NotificationObserver,
                  public extensions::IconImage::Observer,
                  public content::WebContentsDelegate {
 public:
  enum Mode { NORMAL, FOLLOW_CURSOR };

  // Takes |url| as string instead of GURL because resolving GURL requires
  // |extension|. As the client code already passes in |extension|, it'd be
  // better to simply the client code.
  // |opener_render_frame_host| is the RenderFrameHost from where the IME window
  // is opened so that the security origin can be correctly set.
  ImeWindow(Profile* profile,
            const extensions::Extension* extension,
            content::RenderFrameHost* opener_render_frame_host,
            const std::string& url,
            Mode mode,
            const gfx::Rect& bounds);

  // Methods delegate to ImeNativeWindow.
  void Show();
  void Hide();
  void Close();
  void SetBounds(const gfx::Rect& bounds);
  // Aligns the follow-cursor window to the given cursor bounds.
  // If no follow-cursor window is at present, this method does nothing.
  void FollowCursor(const gfx::Rect& cursor_bounds);

  // Gets the web contents' frame ID, which is used to get the JS 'window'
  // object in the custom bindings.
  int GetFrameId() const;

  // The callback that is called when the native window has been destroyed.
  void OnWindowDestroyed();

  void AddObserver(ImeWindowObserver* observer);
  void RemoveObserver(ImeWindowObserver* observer);

  // Getters.
  Mode mode() const { return mode_; }
  const std::string& title() const { return title_; }
  const extensions::IconImage* icon() const { return icon_.get(); }
  const ImeNativeWindow* ime_native_window() const { return native_window_; }

  // extensions::IconImage::Observer:
  void OnExtensionIconImageChanged(extensions::IconImage* image) override;

 private:
  ~ImeWindow() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::WebContentsDelegate overrides.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::WebDragOperationsMask operations_allowed) override;
  void CloseContents(content::WebContents* source) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;

  // Creates the native window.
  ImeNativeWindow* CreateNativeWindow(ImeWindow* ime_window,
                                      const gfx::Rect& bounds,
                                      content::WebContents* contents);

  content::NotificationRegistrar registrar_;

  // The mode of this IME window which is either normal or follow-cursor.
  // The follow-cursor window has the non client view on the left instead
  // of top, and its position can auto-change according to the text cursor
  // or composition.
  Mode mode_;

  // The window title which is shown in the non client view.
  std::string title_;

  // The window icon which is shown in the non client view.
  std::unique_ptr<extensions::IconImage> icon_;

  // The web contents for the IME window page web UI.
  std::unique_ptr<content::WebContents> web_contents_;

  ImeNativeWindow* native_window_;  // Weak, it does self-destruction.

  base::ObserverList<ImeWindowObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(ImeWindow);
};

}  // namespace ui

#endif  // CHROME_BROWSER_UI_INPUT_METHOD_IME_WINDOW_H_

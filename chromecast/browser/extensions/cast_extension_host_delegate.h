// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_HOST_DELEGATE_H_
#define CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_HOST_DELEGATE_H_

#include "extensions/browser/extension_host_delegate.h"

namespace extensions {

// A minimal ExtensionHostDelegate.
class CastExtensionHostDelegate : public ExtensionHostDelegate {
 public:
  CastExtensionHostDelegate();

  CastExtensionHostDelegate(const CastExtensionHostDelegate&) = delete;
  CastExtensionHostDelegate& operator=(const CastExtensionHostDelegate&) =
      delete;

  ~CastExtensionHostDelegate() override;

  // ExtensionHostDelegate implementation.
  void OnExtensionHostCreated(content::WebContents* web_contents) override;
  void OnMainFrameCreatedForBackgroundPage(ExtensionHost* host) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager() override;
  void CreateTab(std::unique_ptr<content::WebContents> web_contents,
                 const std::string& extension_id,
                 WindowOpenDisposition disposition,
                 const gfx::Rect& initial_rect,
                 bool user_gesture) override;
  void ProcessMediaAccessRequest(content::WebContents* web_contents,
                                 const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback,
                                 const Extension* extension) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type,
                                  const Extension* extension) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_HOST_DELEGATE_H_

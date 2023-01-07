// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEB_CONTENTS_CLOSE_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEB_CONTENTS_CLOSE_HANDLER_DELEGATE_H_

// WebContentsCloseHandler delegate.
class WebContentsCloseHandlerDelegate {
 public:
  // Invoked to clone the layers of the WebContents. Should do nothing if there
  // is already a clone (eg CloneWebContentsLayer() has been invoked without a
  // DestroyClonedLayer()) or no WebContents. It is expected that when this is
  // invoked the cloned layer tree is drawn on top of the existing WebContents.
  virtual void CloneWebContentsLayer() = 0;

  // Invoked to destroy the cloned layer tree. This may be invoked when there is
  // no cloned layer tree.
  virtual void DestroyClonedLayer() = 0;

 protected:
  virtual ~WebContentsCloseHandlerDelegate() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEB_CONTENTS_CLOSE_HANDLER_DELEGATE_H_

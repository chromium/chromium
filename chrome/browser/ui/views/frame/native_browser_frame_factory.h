// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_FACTORY_H_

class BrowserFrame;
class BrowserView;
class NativeBrowserFrame;

// Factory for creating a NativeBrowserFrame.
class NativeBrowserFrameFactory {
 public:
  NativeBrowserFrameFactory(const NativeBrowserFrameFactory&) = delete;
  NativeBrowserFrameFactory& operator=(const NativeBrowserFrameFactory&) =
      delete;

  // Construct a platform-specific implementation of this interface.
  static NativeBrowserFrame* CreateNativeBrowserFrame(
      BrowserFrame* browser_frame,
      BrowserView* browser_view);

  // Sets the factory. Takes ownership of |new_factory|, deleting existing
  // factory. Use null to go back to default factory.
  static void Set(NativeBrowserFrameFactory* new_factory);

  virtual NativeBrowserFrame* Create(BrowserFrame* browser_frame,
                                     BrowserView* browser_view);

 protected:
  NativeBrowserFrameFactory() {}
  virtual ~NativeBrowserFrameFactory() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_FACTORY_H_

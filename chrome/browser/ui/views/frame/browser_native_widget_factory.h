// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_FACTORY_H_

class BrowserWidget;
class BrowserView;
class BrowserNativeWidget;

// Factory for creating a BrowserNativeWidget.
class BrowserNativeWidgetFactory {
 public:
  BrowserNativeWidgetFactory(const BrowserNativeWidgetFactory&) = delete;
  BrowserNativeWidgetFactory& operator=(const BrowserNativeWidgetFactory&) =
      delete;

  // Construct a platform-specific implementation of this interface.
  static BrowserNativeWidget* CreateBrowserNativeWidget(
      BrowserWidget* browser_widget,
      BrowserView* browser_view);

  // Sets the factory. Takes ownership of |new_factory|, deleting existing
  // factory. Use null to go back to default factory.
  static void Set(BrowserNativeWidgetFactory* new_factory);

  virtual BrowserNativeWidget* Create(BrowserWidget* browser_widget,
                                      BrowserView* browser_view);

 protected:
  BrowserNativeWidgetFactory() = default;
  virtual ~BrowserNativeWidgetFactory() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_FACTORY_H_

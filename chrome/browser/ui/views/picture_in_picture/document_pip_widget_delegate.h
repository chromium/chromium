// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_WIDGET_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_WIDGET_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class FrameView;
class Widget;
}  // namespace views

class DocumentPipContentsView;
class DocumentPipHost;

// DocumentPipWidgetDelegate is the WidgetDelegate for the standalone Document
// Picture-in-Picture widget. It creates the contents view
// (DocumentPipContentsView) that hosts the child WebContents.
//
// This class replaces BrowserView's role as widget delegate for the PiP window.
class DocumentPipWidgetDelegate : public views::WidgetDelegate {
 public:
  // `host` must outlive this delegate. Takes ownership of `child_web_contents`
  // and transfers it to the inner WebView.
  DocumentPipWidgetDelegate(
      DocumentPipHost* host,
      std::unique_ptr<content::WebContents> child_web_contents);

  DocumentPipWidgetDelegate(const DocumentPipWidgetDelegate&) = delete;
  DocumentPipWidgetDelegate& operator=(const DocumentPipWidgetDelegate&) =
      delete;

  ~DocumentPipWidgetDelegate() override;

  // Returns the contents view, or nullptr if the view tree has been torn down.
  DocumentPipContentsView* GetDocumentPipContentsView();

  // views::WidgetDelegate:
  std::unique_ptr<views::FrameView> CreateFrameView(
      views::Widget* widget) override;

  base::WeakPtr<DocumentPipWidgetDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Owns this delegate and outlives it.
  const raw_ref<DocumentPipHost> host_;

  base::WeakPtrFactory<DocumentPipWidgetDelegate> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_WIDGET_DELEGATE_H_

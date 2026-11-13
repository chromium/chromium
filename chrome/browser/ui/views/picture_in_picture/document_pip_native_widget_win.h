// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_NATIVE_WIDGET_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_NATIVE_WIDGET_WIN_H_

#include <memory>

#include "ui/views/widget/widget.h"

std::unique_ptr<views::Widget> CreateDocumentPipWidgetWin(
    views::Widget::InitParams& params);

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_NATIVE_WIDGET_WIN_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CONTEXT_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CONTEXT_UTIL_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}  // namespace content

namespace ui {
struct AssistantTree;
}  // namespace ui

using RequestAssistantStructureCallback = base::OnceCallback<void(
    ax::mojom::AssistantExtraPtr,       // assistant_extra
    std::unique_ptr<ui::AssistantTree>  // assistant_tree
    )>;

void RequestAssistantStructureForActiveBrowserWindow(
    RequestAssistantStructureCallback callback);

void RequestAssistantStructureForWebContentsForTesting(
    content::WebContents* web_contents,
    RequestAssistantStructureCallback callback);

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CONTEXT_UTIL_H_

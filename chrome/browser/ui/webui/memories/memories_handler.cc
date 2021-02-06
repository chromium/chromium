// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memories/memories_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

MemoriesHandler::MemoriesHandler(
    mojo::PendingReceiver<memories::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  DCHECK(web_contents_);
}

MemoriesHandler::~MemoriesHandler() = default;

void MemoriesHandler::SetPage(
    mojo::PendingRemote<memories::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void MemoriesHandler::GetSampleMemory(MemoryCallback callback) {
  auto memory = memories::mojom::Memory::New();
  std::move(callback).Run(std::move(memory));
}

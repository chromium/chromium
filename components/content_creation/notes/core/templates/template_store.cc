// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_store.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"

namespace content_creation {

namespace {

const char kSourceSerifProFontName[] = "Source Serif Pro";

const ARGBColor kDarkGrayColor = 0xFF202124;
const ARGBColor kWhiteColor = 0xFFFFFFFF;
const ARGBColor kSlightlyTransparentWhiteColor = 0xB3FFFFFF;

NoteTemplate GetClassicTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kClassic, "Classic",
      Background(/*color=*/kDarkGrayColor),
      TextStyle(kSourceSerifProFontName, kWhiteColor, /*all_caps=*/false),
      /*footer_style=*/{kSlightlyTransparentWhiteColor});
}

}  // namespace

TemplateStore::TemplateStore()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})) {}

TemplateStore::~TemplateStore() = default;

void TemplateStore::GetTemplates(GetTemplatesCallback callback) {
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&TemplateStore::BuildTemplates, base::Unretained(this)),
      base::BindOnce(&TemplateStore::OnTemplatesReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::vector<NoteTemplate> TemplateStore::BuildTemplates() {
  // TODO (crbug.com/1194168): Use localized names.
  return {GetClassicTemplate()};
}

void TemplateStore::OnTemplatesReceived(
    GetTemplatesCallback callback,
    std::vector<NoteTemplate> note_templates) {
  std::move(callback).Run(note_templates);
}

}  // namespace content_creation

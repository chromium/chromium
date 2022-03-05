// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_store.h"

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "base/task/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/notes/core/note_prefs.h"
#include "components/content_creation/notes/core/templates/note_template.h"
#include "components/content_creation/notes/core/templates/template_constants.h"
#include "components/content_creation/notes/core/templates/template_fetcher.h"
#include "components/content_creation/notes/core/templates/template_types.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content_creation {

TemplateStore::TemplateStore(
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})),
      pref_service_(pref_service) {
  fetcher_ = std::make_unique<TemplateFetcher>(url_loader);
}

TemplateStore::~TemplateStore() = default;

void TemplateStore::FetchTemplates(GetTemplatesCallback callback) {
  fetcher_->Start(base::BindOnce(&TemplateStore::OnFetchTemplateComplete,
                                 base::Unretained(this), std::move(callback)));
}

void TemplateStore::GetTemplates(GetTemplatesCallback callback) {
  if (IsDynamicTemplatesEnabled()) {
    FetchTemplates(std::move(callback));
  } else {
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&TemplateStore::BuildDefaultTemplates,
                       base::Unretained(this)),
        base::BindOnce(&TemplateStore::OnTemplatesReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void TemplateStore::OnFetchTemplateComplete(GetTemplatesCallback callback,
                                            std::string response_body) {
  OnTemplatesReceived(std::move(callback),
                      ParseTemplatesFromString(response_body));
}

std::vector<NoteTemplate> TemplateStore::BuildDefaultTemplates() {
  std::vector<NoteTemplate> templates = {
      GetClassicTemplate(),  GetFriendlyTemplate(),   GetFreshTemplate(),
      GetPowerfulTemplate(), GetImpactfulTemplate(),  GetLovelyTemplate(),
      GetGroovyTemplate(),   GetMonochromeTemplate(), GetBoldTemplate(),
      GetDreamyTemplate()};

  return templates;
}

std::vector<NoteTemplate> TemplateStore::ParseTemplatesFromString(
    std::string response_body) {
  // TODO(graysonlafleur): implement dynamic templates here
  return BuildDefaultTemplates();
}

void TemplateStore::OnTemplatesReceived(
    GetTemplatesCallback callback,
    std::vector<NoteTemplate> note_templates) {
  if (!IsRandomizeOrderEnabled()) {
    std::move(callback).Run(note_templates);
    return;
  }

  // Have to run the shuffling logic in the result callback as PrefService
  // checks which sequence it's being invoked on.
  auto template_order_opt = prefs::TryGetRandomOrder(pref_service_);
  if (template_order_opt) {
    // Update the list of templates to match the stored order.
    base::flat_map<NoteTemplateIds, NoteTemplate> templates_map;
    for (const NoteTemplate& note_template : note_templates) {
      templates_map.insert({note_template.id(), note_template});
    }

    note_templates.clear();

    for (const NoteTemplateIds id : *template_order_opt) {
      auto template_it = templates_map.find(id);
      if (template_it != templates_map.end()) {
        note_templates.push_back(template_it->second);
      }
    }
  } else {
    // Create a random order and store it.
    base::RandomShuffle(note_templates.begin(), note_templates.end());

    std::vector<NoteTemplateIds> template_order;
    for (const NoteTemplate& note_template : note_templates) {
      template_order.push_back(note_template.id());
    }

    prefs::SetRandomOrder(pref_service_, template_order);
  }

  std::move(callback).Run(note_templates);
}

}  // namespace content_creation

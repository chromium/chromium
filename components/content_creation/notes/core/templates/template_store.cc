// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_store.h"

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/notes/core/note_prefs.h"
#include "components/content_creation/notes/core/templates/note_template.h"
#include "components/content_creation/notes/core/templates/template_constants.h"
#include "components/content_creation/notes/core/templates/template_fetcher.h"
#include "components/content_creation/notes/core/templates/template_types.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content_creation {

namespace {

bool ConvertProtoDateToTime(proto::Date date, base::Time& time_date) {
  base::Time::Exploded exploded_date = {
      /*year=*/date.year(),
      /*month=*/date.month(),
      /*day_of_week=*/0,
      /*day_of_month=*/date.day(),
      /*hour=*/0,
      /*minute=*/0,
      /*second=*/0,
      /*millisecond=*/0,
  };

  return base::Time::FromLocalExploded(exploded_date, &time_date);
}

std::string FetchTemplatesFromFile(base::FilePath local_path) {
  std::string data;

  if (!base::ReadFileToString(local_path, &data)) {
    return "";
  }

  return data;
}

}  // namespace

TemplateStore::TemplateStore(
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader,
    std::string country_code)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})),
      pref_service_(pref_service),
      country_code_(country_code) {
  fetcher_ = std::make_unique<TemplateFetcher>(url_loader);
}

TemplateStore::~TemplateStore() = default;

void TemplateStore::FetchTemplates(GetTemplatesCallback callback) {
  fetcher_->Start(base::BindOnce(&TemplateStore::OnFetchTemplateComplete,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback)));
}

void TemplateStore::GetTemplates(GetTemplatesCallback callback) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kLocalDynamicTemplatesForTesting)) {
    OnFetchTemplateComplete(
        std::move(callback),
        FetchTemplatesFromFile(
            base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
                kLocalDynamicTemplatesForTesting)));
    return;
  }

  if (IsDynamicTemplatesEnabled()) {
    FetchTemplates(std::move(callback));
  } else {
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&TemplateStore::BuildDefaultTemplates),
        base::BindOnce(&TemplateStore::OnTemplatesReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void TemplateStore::OnFetchTemplateComplete(GetTemplatesCallback callback,
                                            std::string response_body) {
  OnTemplatesReceived(std::move(callback),
                      ParseTemplatesFromString(response_body));
}

// static
std::vector<NoteTemplate> TemplateStore::BuildDefaultTemplates() {
  std::vector<NoteTemplate> templates = {
      GetClassicTemplate(),  GetFriendlyTemplate(),   GetFreshTemplate(),
      GetPowerfulTemplate(), GetImpactfulTemplate(),  GetLovelyTemplate(),
      GetGroovyTemplate(),   GetMonochromeTemplate(), GetBoldTemplate(),
      GetDreamyTemplate()};

  return templates;
}

bool TemplateStore::TemplateDateAvailable(proto::CollectionItem current_item,
                                          base::Time today) {
  base::Time activation;
  base::Time expiration;

  if (current_item.has_activation() &&
      (!ConvertProtoDateToTime(current_item.activation(), activation) ||
       today < activation)) {
    return false;
  }

  if (current_item.has_expiration() &&
      (!ConvertProtoDateToTime(current_item.expiration(), expiration) ||
       today >= expiration)) {
    return false;
  }

  return true;
}

bool TemplateStore::TemplateLocationAvailable(
    proto::CollectionItem current_item) {
  // If there are no locations set, the template is considered available for all
  // locations.
  if (current_item.geo_size() == 0) {
    return true;
  }

  // If a location is set, but the user's country code is empty, the template
  // will not be available for the user.
  if (country_code_.empty()) {
    return false;
  }

  for (std::string template_location : current_item.geo()) {
    if (country_code_ == template_location) {
      return true;
    }
  }

  return false;
}

std::vector<NoteTemplate> TemplateStore::ParseTemplatesFromString(
    std::string response_body) {
  std::vector<NoteTemplate> templates = {};
  proto::Collection collection;
  // Time is set here so that all templates will be compared against the exact
  // same date.
  base::Time today = base::Time::NowFromSystemTime();

  if (!collection.ParseFromString(response_body)) {
    return BuildDefaultTemplates();
  }

  int numTemplates = 0;

  for (int i = 0; i < collection.collectionitems_size() &&
                  numTemplates < collection.max_template_number();
       i++) {
    proto::CollectionItem current_item = collection.collectionitems(i);

    if (!TemplateDateAvailable(current_item, today)) {
      continue;
    }

    if (!TemplateLocationAvailable(current_item)) {
      continue;
    }

    templates.push_back(NoteTemplate(current_item.notetemplate()));
    numTemplates++;
  }

  if (templates.size() == 0) {
    return BuildDefaultTemplates();
  }

  return templates;
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

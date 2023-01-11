// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_STORE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_STORE_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/content_creation/notes/core/templates/template_fetcher.h"
#include "components/content_creation/notes/core/templates/template_storage.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;

namespace base {
class Time;
}

namespace content_creation {

class NoteTemplate;

using GetTemplatesCallback =
    base::OnceCallback<void(std::vector<NoteTemplate>)>;

// If set, it will use data from a local file rather than gstatic to serve
// templates to the user. Used to test updated template files before committing
// to gstatic.
constexpr char kLocalDynamicTemplatesForTesting[] =
    "local-dynamic-templates-for-testing";

// Instance in charge of generating the ordered list of note templates to be
// offered to the user.
class TemplateStore {
 public:
  explicit TemplateStore(
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader,
      std::string country_code);
  virtual ~TemplateStore();

  // Not copyable or movable.
  TemplateStore(const TemplateStore&) = delete;
  TemplateStore& operator=(const TemplateStore&) = delete;

  // Checks whether given template should be available based on activation and
  // expiration dates.
  bool TemplateDateAvailable(proto::CollectionItem current_template,
                             base::Time today);

  // Checks whether given template should be available based on the location of
  // the user.
  bool TemplateLocationAvailable(proto::CollectionItem current_template);

  // Calls Start() in TemplateFetcher to do a GET request and send the
  // data from the URL to OnFetchTemplateComplete.
  void FetchTemplates(GetTemplatesCallback callback);

  // Gets the set of templates to be used for generating stylized notes. Will
  // invoke |callback| with the results.
  virtual void GetTemplates(GetTemplatesCallback callback);

 protected:
  void OnFetchTemplateComplete(GetTemplatesCallback callback,
                               std::string response_body);

  // Function which generates the ordered list of default templates to be
  // offered to the user.
  static std::vector<NoteTemplate> BuildDefaultTemplates();

  // Function which generates the ordered list of pulled templates to be
  // offered to the user.
  std::vector<NoteTemplate> ParseTemplatesFromString(std::string data);

  // This function is invoked when the store has successfully built the list
  // of |note_templates|, and will send them to the user via |callback|.
  void OnTemplatesReceived(GetTemplatesCallback callback,
                           std::vector<NoteTemplate> note_templates);

  std::unique_ptr<TemplateFetcher> fetcher_;

  // Task runner delegating tasks to the ThreadPool.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<PrefService> pref_service_;

  std::string country_code_;

  base::WeakPtrFactory<TemplateStore> weak_ptr_factory_{this};
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_STORE_H_

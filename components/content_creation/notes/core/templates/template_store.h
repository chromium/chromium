// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_STORE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_STORE_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/supports_user_data.h"

class PrefService;

namespace content_creation {

class NoteTemplate;

using GetTemplatesCallback =
    base::OnceCallback<void(std::vector<NoteTemplate>)>;

// Instance in charge of generating the ordered list of note templates to be
// offered to the user.
class TemplateStore {
 public:
  explicit TemplateStore(PrefService* pref_service);
  virtual ~TemplateStore();

  // Not copyable or movable.
  TemplateStore(const TemplateStore&) = delete;
  TemplateStore& operator=(const TemplateStore&) = delete;

  // Gets the set of templates to be used for generating stylized notes. Will
  // invoke |callback| with the results.
  virtual void GetTemplates(GetTemplatesCallback callback);

 protected:
  // Function which generates the ordered list of templates to be offered to
  // the user.
  std::vector<NoteTemplate> BuildTemplates();

  // This function is invoked when the store has successfully built the list
  // of |note_templates|, and will send them to the user via |callback|.
  void OnTemplatesReceived(GetTemplatesCallback callback,
                           std::vector<NoteTemplate> note_templates);

  // Task runner delegating tasks to the ThreadPool.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  PrefService* pref_service_;

  base::WeakPtrFactory<TemplateStore> weak_ptr_factory_{this};
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_STORE_H_
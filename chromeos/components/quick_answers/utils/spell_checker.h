// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_SPELL_CHECKER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_SPELL_CHECKER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/public/mojom/spell_check.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace quick_answers {

// Utility class for spell check.
class SpellChecker : public QuickAnswersStateObserver {
 public:
  using CheckSpellingCallback = base::OnceCallback<void(bool)>;

  explicit SpellChecker(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  SpellChecker(const SpellChecker&) = delete;
  SpellChecker& operator=(const SpellChecker&) = delete;

  ~SpellChecker() override;

  // Check spelling of the given word, run |callback| with true if the word is
  // spelled correctly. Virtual for testing.
  virtual void CheckSpelling(const std::string& word,
                             CheckSpellingCallback callback);

  // QuickAnswersStateObserver:
  void OnSettingsEnabled(bool enabled) override;
  void OnApplicationLocaleReady(const std::string& locale) override;

  base::WeakPtr<SpellChecker> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void InitializeDictionary();
  void InitializeSpellCheckService();

  void OnSimpleURLLoaderComplete(std::unique_ptr<std::string> response_body);

  void OnDictionaryCreated(
      mojo::PendingRemote<mojom::SpellCheckDictionary> dictionary);

  void MaybeRetryInitialize();

  // The reply points for PostTaskAndReplyWithResult.
  void OnPathExistsComplete(bool path_exists);
  void OnSaveDictionaryDataComplete(bool dictionary_saved);
  void OnOpenDictionaryFileComplete(base::File file);

  // Task runner where the file operations takes place.
  scoped_refptr<base::SequencedTaskRunner> const task_runner_;

  // Whether the Quick answers feature is enabled in settings.
  bool feature_enabled_ = false;

  // Whether the spell check dictionary has been successfully initialized.
  bool dictionary_initialized_ = false;

  // Number of retries used for initializing the spell check service.
  int num_retries_ = 0;

  base::FilePath dictionary_file_path_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  mojo::Remote<mojom::SpellCheckService> service_;
  mojo::Remote<mojom::SpellCheckDictionary> dictionary_;

  base::ScopedObservation<QuickAnswersState, QuickAnswersStateObserver>
      quick_answers_state_observation_{this};

  base::WeakPtrFactory<SpellChecker> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_SPELL_CHECKER_H_

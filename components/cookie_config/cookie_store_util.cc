// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cookie_config/cookie_store_util.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"

namespace cookie_config {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
namespace {

void OnOsCryptReadyOnUi(
    base::OnceCallback<void(os_crypt_async::Encryptor)> callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    os_crypt_async::Encryptor encryptor) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(encryptor)));
}

void InitOnUi(base::OnceCallback<void(os_crypt_async::Encryptor)> callback,
              os_crypt_async::OSCryptAsync* os_crypt_async,
              scoped_refptr<base::SequencedTaskRunner> task_runner) {
  os_crypt_async->GetInstance(
      base::BindOnce(&OnOsCryptReadyOnUi, std::move(callback),
                     std::move(task_runner)),
      os_crypt_async::Encryptor::Option::kEncryptSyncCompat);
}

// Use the operating system's mechanisms to encrypt cookies before writing
// them to persistent store.  Currently this only is done with desktop OS's
// because ChromeOS and Android already protect the entire profile contents.
class CookieOSCryptoDelegate : public net::CookieCryptoDelegate {
 public:
  CookieOSCryptoDelegate(
      os_crypt_async::OSCryptAsync* os_crypt_async,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  CookieOSCryptoDelegate(const CookieOSCryptoDelegate&) = delete;
  CookieOSCryptoDelegate& operator=(const CookieOSCryptoDelegate&) = delete;

  ~CookieOSCryptoDelegate() override;

  // net::CookieCryptoDelegate implementation:
  void Init(base::OnceClosure callback) override;
  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;
  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;

 private:
  void OnOsCryptReady(os_crypt_async::Encryptor encryptor);

  raw_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  std::optional<os_crypt_async::Encryptor> encryptor_;

  bool initializing_ = false;
  std::vector<base::OnceClosure> init_callbacks_;

  base::WeakPtrFactory<CookieOSCryptoDelegate> weak_ptr_factory_{this};
};

CookieOSCryptoDelegate::CookieOSCryptoDelegate(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : os_crypt_async_(os_crypt_async), ui_task_runner_(ui_task_runner) {}

CookieOSCryptoDelegate::~CookieOSCryptoDelegate() = default;

void CookieOSCryptoDelegate::Init(base::OnceClosure callback) {
  if (encryptor_.has_value()) {
    std::move(callback).Run();
    return;
  }

  init_callbacks_.emplace_back(std::move(callback));
  if (initializing_) {
    return;
  }
  initializing_ = true;

  // PostTaskAndReplyWithResult can't be used here because
  // OSCryptAsync::GetInstance() is async.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InitOnUi,
                     base::BindOnce(&CookieOSCryptoDelegate::OnOsCryptReady,
                                    weak_ptr_factory_.GetWeakPtr()),
                     os_crypt_async_,
                     base::SequencedTaskRunner::GetCurrentDefault()));
  os_crypt_async_ = nullptr;
}

bool CookieOSCryptoDelegate::EncryptString(const std::string& plaintext,
                                           std::string* ciphertext) {
  CHECK(encryptor_) << "EncryptString called before Init completed";
  return encryptor_->EncryptString(plaintext, ciphertext);
}

bool CookieOSCryptoDelegate::DecryptString(const std::string& ciphertext,
                                           std::string* plaintext) {
  CHECK(encryptor_) << "DecryptString called before Init completed";
  return encryptor_->DecryptString(ciphertext, plaintext);
}

void CookieOSCryptoDelegate::OnOsCryptReady(
    os_crypt_async::Encryptor encryptor) {
  encryptor_ = std::move(encryptor);
  initializing_ = false;
  for (auto& callback : init_callbacks_) {
    std::move(callback).Run();
  }
  init_callbacks_.clear();
}

}  // namespace

std::unique_ptr<net::CookieCryptoDelegate> GetCookieCryptoDelegate(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  return std::make_unique<CookieOSCryptoDelegate>(os_crypt_async,
                                                  ui_task_runner);
}
#else   // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<net::CookieCryptoDelegate> GetCookieCryptoDelegate(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  return nullptr;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace cookie_config

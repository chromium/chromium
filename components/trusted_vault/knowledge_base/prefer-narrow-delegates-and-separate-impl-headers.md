---
name: trusted_vault/prefer-narrow-delegates-and-separate-impl-headers
rubric: design
severity: should_fix
---

**Rule.** Prefer composition with narrow delegate, factory, and role-specific
storage interfaces (`StorageFileAccess`, `Delegate`,
`LocalRecoveryFactorsFactory`, `PhysicalDeviceStorage`,
`RecoveryFactorRegistrationStorage`, `KeyStorage`,
`ConnectionThrottlingStorage`, `DegradedRecoverabilityStorage`) over
implementation inheritance, expose test-only dependency injection via private
constructors and `CreateForTesting()` factories, and keep concrete `*Impl` class
declarations in separate `*_impl.h` headers rather than in pure interface
headers (`*_connection.h`, `*_fetcher.h`).

**Why.** Mixing production state machine logic with `virtual` I/O hooks for test
subclasses or passing the full `StandaloneTrustedVaultStorage` interface to
collaborators that only need a single storage role weakens encapsulation and
complicates fakes. Separating `*_connection.h` interfaces from `*_impl.h`
headers keeps consumer dependencies minimal and prevents implementation details
from leaking into callers and unit tests.

## Examples

Bad:

```cpp
// In trusted_vault_connection.h:
class TrustedVaultConnection {
 public:
  virtual ~TrustedVaultConnection() = default;
  virtual std::unique_ptr<Request> DownloadNewKeys(...) = 0;
};

// BAD: Concrete *Impl declared inside the public interface header.
class TrustedVaultConnectionImpl : public TrustedVaultConnection { ... };

// BAD: Uses implementation inheritance for file I/O and exposes a public test
// constructor instead of a narrow delegate with CreateForTesting().
class LocalDomainsStorage : public StandaloneTrustedVaultStorage {
 public:
  LocalDomainsStorage();
  virtual trusted_vault_pb::LocalDomainsData ReadFromDisk();
  virtual void WriteToDisk(const trusted_vault_pb::LocalDomainsData& data);
};
```

Good:

```cpp
// In trusted_vault_connection.h (interface only; TrustedVaultConnectionImpl is
// declared in trusted_vault_connection_impl.h):
class TrustedVaultConnection {
 public:
  virtual ~TrustedVaultConnection() = default;
  virtual std::unique_ptr<Request> DownloadNewKeys(...) = 0;
};

// In local_domains_storage.h:
class LocalDomainsStorage : public StandaloneTrustedVaultStorage {
 public:
  class StorageFileAccess {
   public:
    virtual ~StorageFileAccess() = default;
    virtual trusted_vault_pb::LocalDomainsData ReadFromDisk() = 0;
    virtual void WriteToDisk(
        const trusted_vault_pb::LocalDomainsData& data) = 0;
  };

  static std::unique_ptr<LocalDomainsStorage> Create(
      const base::FilePath& base_dir,
      scoped_refptr<const os_crypt_async::Encryptor> encryptor);
  static std::unique_ptr<LocalDomainsStorage> CreateForTesting(
      std::unique_ptr<StorageFileAccess> file_access);

 private:
  explicit LocalDomainsStorage(std::unique_ptr<StorageFileAccess> file_access);
};
```

---
name: enterprise/keep-cross-platform-delegates-pure-virtual
rubric: design
severity: should_fix
---

**Rule.** In cross-platform enterprise and device-signals delegate interfaces
(such as `device_signals::PlatformDelegate`), declare core interface methods
that every supported platform must implement as pure virtual (`= 0`). Only
provide a base `return std::nullopt;` default implementation for documented
OS-specific optional signals or capabilities that do not exist on all platforms.

**Why.** Pure virtual methods on cross-platform enterprise delegates force every
platform implementation (Windows, Mac, Linux, ChromeOS, Android, iOS) to
explicitly implement core functionality at compile time instead of silently
returning empty data at runtime, while still allowing documented `std::nullopt`
defaults for genuinely OS-specific signals (such as `GetProductMetadata` or
`GetSigningCertificatesPublicKeys`).

## Examples

Bad:

```cpp
class PlatformDelegate {
 public:
  virtual ~PlatformDelegate() = default;

  // Core filesystem check silently defaults to false if a platform forgets to
  // override it:
  virtual bool PathIsReadable(const base::FilePath& file_path) const {
    return false;
  }
};
```

Good:

```cpp
class PlatformDelegate {
 public:
  virtual ~PlatformDelegate() = default;

  // Core capability required on every platform:
  virtual bool PathIsReadable(const base::FilePath& file_path) const = 0;

  // Documented OS-specific optional signal (returns std::nullopt in
  // platform_delegate.cc if unsupported on the current platform):
  virtual std::optional<SigningCertificatesPublicKeys>
  GetSigningCertificatesPublicKeys(const base::FilePath& file_path);
};
```

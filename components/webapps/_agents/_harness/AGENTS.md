# Universal AI Agent Guidelines for Chromium

Universal, subsystem-agnostic engineering standards and security constraints for
AI agents operating in Chromium.

## Canonical Documentation & Guidelines

### 1. C++ & Architecture Guidelines

- **C++ Style Guide:** `styleguide/c++/c++.md`
- **C++ Do's and Don'ts:** `styleguide/c++/c++-dos-and-donts.md`
- **Threading & Tasks:** `docs/threading_and_tasks.md`
- **MiraclePtr (`raw_ptr<T>`):** `base/memory/raw_ptr.md`

### 2. Mojo IPC Security & Validation

- **Mojo Security Guide:** `docs/security/mojo.md`
- **Rule of 2:** `docs/security/rule-of-2.md`
- **Mojo C++ Bindings:** `mojo/public/cpp/bindings/README.md`
- Treat renderer inputs as untrusted; perform validation and filesystem/token
  management exclusively in the browser process.

### 3. Testing & Verification Standards

- **Testing in Chromium:** `docs/testing/testing_in_chromium.md`
- **Autotest Runner Guide:** `tools/autotest/README.md`
- **Async & TaskEnvironment Testing:** `docs/threading_and_tasks_testing.md`

### 4. Markdown & Documentation Style

- **Markdown Style Guide:** `styleguide/markdown/markdown.md`
- Wrap all markdown prose to 80 columns (`git cl format`).
- Documentation integrity: maintain canonical human READMEs; agent rules act as
  thin pointers rather than duplicating knowledge.

## Standard Templates & Workflows

- [Technical Designs Template](DESIGNS.md) |
  [Execution Plans Template](PLANS.md) | [Adversarial Reviews Spec](REVIEWS.md)

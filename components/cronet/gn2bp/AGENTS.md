# GN2BP Agent Documentation

## Overview

`gn2bp` is the toolchain used to bridge Chromium's GN build system with Android's Soong build system (`Android.bp`). Its primary goal is to take a set of target GN labels (such as Cronet), parse their descriptions, and translate them into `Android.bp` modules. Finally, it uses Copybara to automatically import the Chromium code and the generated `Android.bp` files into the Android repository (`platform/external/cronet`).

This tool is especially tailored for Cronet's import into Android, meaning it contains many Cronet-specific and Android-specific workarounds. See [go/cronet-platform](http://go/cronet-platform).

## High-Level Workflow

The entry point for the process is typically `run_gn2bp.py`. The overall pipeline looks like this:

1. **Release Validation**: `run_gn2bp.py` fetches known breakages from `breakages.json` and verifies that the current Git history doesn't contain a bad commit without its corresponding fix (can be skipped locally with `--skip-release-validation`).
2. **GN Config and Build Scripts Execution**: `generate_build_scripts_output.py` is invoked to compile and run any Rust build scripts (`build.rs`) for the root GN target groups (`//components/cronet/android:gn2bp_production_targets` and `//components/cronet/android:gn2bp_test_targets`). Soong does not support executing Rust build scripts dynamically at build time, so Chromium pre-evaluates them and saves the resulting `cargo_flags` and other generated Rust code into `gn2bp_rust_build_script_outputs/`.
3. **GN Desc Evaluation**: `gn desc ... --format=json` is run for each architecture (`x86`, `x64`, `arm`, `arm64`, `riscv64`) plus `host` to extract the full GN dependency graph and target properties.
4. **Target Translation & Copybara Path Generation**: `gen_android_bp.py` processes the GN descriptions using `gn_utils.py`, creates an in-memory `Blueprint` and `Module` representation of each target (`soong_ast.py`, `translators.py`, `action_sanitizers.py`, `translation_config.py`, `context.py`), applies customizations, and writes all generated outputs into `out/gn2bp/`:
   * `out/gn2bp/**/Android.bp`: The generated Soong build files split per directory.
   * `out/gn2bp/copybara_generated_paths.bara.sky`: Auto-generated lists of exact source files (`GENERATED_ORIGIN_FILES`) and directory globs (`GENERATED_ORIGIN_GLOBS`) extracted from the GN graph, generated blueprints, and `README.chromium` metadata.
5. **Additional Files**: Generates licenses (`LICENSE`, `MODULE_LICENSE_*`), BoringSSL rules (`sources.bp`, `sources.mk`), and test configuration (`AndroidTest.xml`) inside `out/gn2bp/`.
6. **Copybara Import**: `run_gn2bp.py` invokes Copybara with `copy.bara.sky`. Copybara loads `out/gn2bp/copybara_generated_paths.bara.sky`, moves `out/gn2bp/` onto the destination channel root (`tot/` or `stable/`), packages the needed files, and pushes them to a local Android checkout or uploads them as a Gerrit changelist (CL) to Android's `platform/external/cronet` project.

## Core Components

### `run_gn2bp.py`
The driver script. It handles concurrency for `gn desc` generation across different architectures, invokes `gen_android_bp.py`, and orchestrates the Copybara import. It supports importing into different channels: `tot` (Tip of Tree) and `stable`.

### `targets.py` & `components/cronet/android/BUILD.gn`
* `components/cronet/android/BUILD.gn` defines the two single-entry-point GN groups evaluated by `gn2bp`:
  * `//components/cronet/android:gn2bp_production_targets`
  * `//components/cronet/android:gn2bp_test_targets`
* `targets.py` references those two root groups (`DEFAULT_TARGETS` and `DEFAULT_TESTS`) and maps the leaf GN targets to their stable top-level Soong module names in `GN_TO_AOSP_TARGET_NAMES`.

### `gn_utils.py`
The GN JSON parser.
* Parses the output of `gn desc` and translates GN targets into `GnParser.Target` objects.
* **`source_set` and `group` Bubbling**: Soong doesn't have a direct equivalent to GN's `source_set`. `gn_utils.py` fakes it by bubbling up the dependencies, `sources`, `inputs`, `cflags`, `cflags_cc`, `defines`, and `include_dirs` of `source_set` and `group` targets to their dependent linker units (`static_library`, `shared_library`, `executable`, `rust_library`).
* Consolidates architecture-specific arguments, keeping common flags in a `common` arch and pushing architecture-specific flags to their respective arch object.
* Inlines `@response_file` arguments from GN `response_file_contents`.

### `gen_android_bp.py`, `translators.py`, `soong_ast.py`, `translation_config.py`, `context.py`
The core GN to Soong translation layer:
* **`soong_ast.py`**: Defines the in-memory `Blueprint` and `Module` AST representations (for Soong module types like `cc_library_static`, `cc_genrule`, `java_library`, `rust_ffi_static`, `crubit_rust_cpp_library`, etc.).
* **`translation_config.py`**: Implements `builtin_deps` and `replace_deps` to swap out Chromium dependencies for their Android equivalents (e.g., routing `//third_party/zlib:zlib` to Android's `libz`, swapping `androidx` dependencies).
* **`context.py`**: Holds the `TranslationContext` and `create_additional_args()` for per-target overrides.
* **`translators.py`**: Maps `gn_utils.Target` objects to `soong_ast.Module` objects (`create_modules_from_target`).
* **`gen_android_bp.py`**: Orchestrates the overall Blueprint creation, splits the overall blueprint into smaller `Android.bp` files per directory (`rebase_blueprint`) to comply with Android rules (e.g., `third_party/rust/...` crates get their own `Android.bp`), generates `cc_defaults` (`create_cc_defaults_module()`), `libcxx_headers` (`create_libcxx_headers_module()`), and extra modules (`create_extras_modules()`), and writes `out/gn2bp/copybara_generated_paths.bara.sky` (`_write_copybara_generated_paths`).

### Action Sanitizers (in `action_sanitizers.py` and `translators.py`)
GN `action` and `action_foreach` targets execute arbitrary Python scripts. Soong's `genrule` executes shell commands. `action_sanitizers.py` uses `BaseActionSanitizer` subclasses (dispatched via `get_action_sanitizer()` and invoked from `translators.py`) to translate GN script invocations into Soong-compatible bash commands:
* **JNI Generators (`JniZeroActionSanitizer`, `JniRegistrationGeneratorActionSanitizer`)**: Translates JNI generation actions (`jni_zero.py`) into Soong rules. Handles creating proxy classes and placeholder classes, separating them when necessary so placeholder classes don't pollute the classpath.
* **Protos**: GN proto targets are split into two `cc_genrule`s: one for `.h` generation and one for `.cc` generation.
* **CXX, Bindgen & Crubit**: Translates Rust-C++ interop actions (`cxxbridge`, `bindgen`, and `rs_bindings_from_cc` / `crubit_cpp_from_rust`) into the appropriate `rust_bindgen`, `cc_genrule`, or Crubit header/source modules.

### `copy.bara.sky`
The Copybara configuration file. Loads `out/gn2bp/copybara_generated_paths.bara.sky` to determine `origin_files` (what to import from Chromium) and moves `out/gn2bp/` onto the destination root (`tot/` or `stable/`). Defines workflows for both `tot` and `stable` branches and excludes non-essential files to keep the Android checkout slim.

## Language Specific Handling

### Java
Translating GN's `java_library` to Soong requires simulating GN's jar filtering (`jar_included_patterns`/`jar_excluded_patterns`). `translators.py` achieves this by creating three Soong modules for a single filtered GN target:
1. `__unfiltered`: The actual `java_library` that compiles the code.
2. `__filtered`: A `java_genrule` that uses `filter_zip.py` to strip out excluded classes.
3. The top-level module: A `java_library` acting as a dependency holder that statically links the filtered jar.

### Rust
* **Bindgen**: Mapped to `rust_bindgen`.
* **Crubit**: `crubit_cpp_from_rust` targets generate C++ header bindings from Rust crates via `rs_bindings_from_cc` `cc_genrule`s.
* **Crates**: Mapped to `rust_ffi_static`, `rust_proc_macro`, or `rust_binary`. The `crate_name` is extracted, and target names are hashed to avoid file path length limits in Soong.
* **Build Scripts**: Because Soong doesn't run `build.rs` scripts dynamically, `generate_build_scripts_output.py` pre-evaluates them and saves outputs in `gn2bp_rust_build_script_outputs/`.
* **Crubit (`cpp_api_from_rust`)**: Mapped to Soong's native `crubit_cpp_from_rust` module. Crubit's C++ support library lives in `//third_party/rust-toolchain`, which is not imported; instead, `gn_utils.py` collapses the GN entry points (`//build/rust/crubit:crubit` and `:cpp_api_from_rust_bindings_cpp_deps`) into a single synthetic `cc_library_static` (`CRUBIT_CPP_SUPPORT_TARGET`) that compiles prebuilts/rust-toolchain's `crubit_support_cc_srcs` filegroup with Cronet's own flags and libc++. `crubit_cpp_from_rust` modules link it via `static_libs` and set `no_linked_cc_support: true` to opt out of Android's prebuilt `libcrubit_support_cc`.

### C++
* Extracts per-target `cflags`, `cflags_cc`, `ldflags`, `defines`, `-isystem` flags, and `include_dirs`, and attaches them to the proper targets.
* Removes flags that are forbidden, handled by Soong, or incompatible with Android's toolchain (see `_CFLAGS_TO_REMOVE` in `translators.py`), and configures global baseline flags in `create_cc_defaults_module()` (`gen_android_bp.py`).
* Derives `libcxx_headers` from `//buildtools/third_party/libc++:libc++` (`inputs`, `-isystem`, and `include_dirs`).

## Important Concepts & Gotchas

* **FIPS Compliance / Libcrypto Stripping**: Cronet links against BoringSSL. For FIPS compliance in Android, `libcrypto` must be a shared library (`libcrypto_httpengine.so`). Because this prevents dead-code elimination, `gn2bp` uses a two-step linker script generation (`generate_libcronet_libcrypto_symbol_linker_script` in `gen_android_bp.py`) to extract only the symbols Cronet actually uses, pruning the final `libcrypto` output.
* **`create_extras_modules()`**: Used in `gen_android_bp.py` (alongside `templates/Android.extras.bp.template`) for manually maintained Soong modules that are not yet autogenerated from GN (e.g., `license`, `jni_headers`, test app wrappers).
* **`GN2BP_MODULE_PREFIX`**: Most generated module names are prefixed with a channel-specific prefix (`tot_cronet_` or `stable_cronet_`) to avoid collisions between the Tip-of-Tree and Stable channel imports inside the Android tree.
* **Rebasing Blueprints**: `gen_android_bp.py` breaks down the single massive `Blueprint` into multiple smaller files (`rebase_blueprint`) and places them near their source files, rebasing file paths accordingly.

---

## How to Tackle Problems & Always Document the "Why"

### 1. Rely on GN as the Single Source of Truth
Whenever a build failure, missing header/source, wrong compiler flag, or Copybara path issue arises:
* **Do NOT default to hardcoding**: Avoid adding ad-hoc file paths, compiler flags, or manual module definitions in `gen_android_bp.py`, `context.py`, `copy.bara.sky`, or `Android.extras.bp.template`.
* **Inspect `gn desc` first**: Check what GN already knows about the target in the `gn desc --format=json` output (`sources`, `inputs`, `public`, `cflags`, `cflags_cc`, `include_dirs`, `configs`, `public_configs`, `all_dependent_configs`, `deps`).
* **Fix the translation layer**: If GN already models the dependency, header, or flag (for example, `inputs` and `-isystem` on `//buildtools/third_party/libc++:libc++`, or per-target `cflags` / `cflags_cc`), teach `gn_utils.py` / `translators.py` / `gen_android_bp.py` to extract and translate it systematically rather than maintaining a parallel hardcoded list in Python or Starlark.

### 2. Respect Cross-Repo & Multi-Channel Constraints (`tot`, `stable`, and AOSP)
In Android's `platform/external/cronet` repository:
* `tot/` and `stable/` live side by side in the same repository and share a single top-level `external/cronet/Android.bp` file outside Copybara's `tot/` and `stable/` directories.
* **Two-Phase Migrations for Top-Level Modules and Shared Files**:
  * If `external/cronet/Android.bp` (or another Android directory) references a module name or file path generated by `gn2bp`, you **cannot** delete or rename that module/file in a single Chromium CL without breaking the import.
  * Instead, use a two-phase migration:
    1. Keep a backward-compatible alias/wrapper in `gn2bp` (marked with a `TODO` and bug/CL link) while landing the new target/structure.
    2. Update `external/cronet/Android.bp` in AOSP (and wait for the change to roll into both `tot` and `stable` if applicable), then remove the compatibility alias in Chromium.
* **Copybara `merge_import` and `chromium.bara.consistency` Gotchas**:
  * Copybara runs in `merge_import` mode using a `chromium.bara.consistency` file in the destination repo.
  * If you want Copybara to **delete** a previously imported file from Android on the next import, remove that file from `origin_files` (so it is no longer imported from Chromium) **without** adding it to `destination_files` exclusions. If you exclude a file from `destination_files`, Copybara treats it as a "destination-only" file and leaves it untouched in Android rather than deleting it!

### 3. Always Document the "Why"
`gn2bp` bridges two very different build systems (GN and Soong) and two different toolchains/repositories (Chromium and Android). Future readers and agents cannot tell whether a workaround is still needed unless the *reason* is documented:
* **In Code Comments**: Whenever you add or touch a flag filter, a `cc_defaults` flag, a `create_additional_args()` override, an `ActionSanitizer` heuristic, or a `TODO`, always document **why** it is needed (e.g., which Soong global `-Werror` flag or Clang version skew triggered it, which GN target structure caused it, or which `crbug.com/...` issue tracks its removal).
* **In Commit Messages**: Clearly state:
  1. **What** was previously hardcoded or broken and **why** it was written that way originally.
  2. **How** the new approach derives the behavior from GN or simplifies the pipeline.
  3. Any companion AOSP CLs or follow-up steps required across `tot` and `stable`.

---

## How to Use as an Agent

### Internal Documentation
If the user is a Googler or has access to Google-internal resources, read [go/cronet-platform](http://go/cronet-platform) for essential context on the Cronet Android platform architecture, CI builders, and import procedures.

### Why Local Checkouts Matter (Cider-G / Cog Limitations)
* In a **Cider-G / Cog workspace**, the Copybara step in `run_gn2bp.py` is **not supported** ([go/cronet-platform](http://go/cronet-platform)); only `--skip-copybara` mode works directly in Cog.
* Furthermore, in sandboxed agent environments, `/usr/bin/git` cannot execute git operations inside a Cog workspace, because Cog's virtual `.git` metadata lives above the sandbox mount boundary.
* Finally, verifying that generated `Android.bp` files actually parse and compile requires an Android platform checkout (`platform/external/cronet`).

### Step 1: Always Ask the User First
Before starting work on any non-trivial `gn2bp` change, **you MUST ask the user**:
1. **Can you grant access to two local checkouts on your machine for fast iteration?**
   * A **traditional Chromium git checkout** (e.g., `/path/to/chromium/src`)
   * A **local Android platform checkout** containing `external/cronet` (e.g., `/path/to/android`)
2. **If the user cannot provide access to both local checkouts**, ask whether you should iterate by **running `--skip-copybara` locally and testing full imports via Chromium's CQ trybot (`cronet-arm64-gn2bp-debug`)**.

---

### Workflow A: Iterating with the Two Local Repositories (Preferred)

When the user provides paths to a local Chromium git checkout (`<CHROMIUM_REPO>`) and a local Android checkout (`<ANDROID_REPO>`):

#### 1. Cog + Local Two-Repo Development Pattern
If your primary workspace is in Cog:
1. **Develop and verify first in `<CHROMIUM_REPO>` and `<ANDROID_REPO>`**: Apply your changes in `<CHROMIUM_REPO>`, run `gn2bp` and Copybara against `<ANDROID_REPO>`, and compile the Android targets to confirm everything works end-to-end.
2. **Sync verified changes back to Cog**: Once verified, apply the exact changes to the Cog workspace using file editing tools (`edit_file` / `write_file`). When building a chain of multiple CLs, bring and commit them one by one in Cog.

#### 3. Fast Iteration on `gn2bp` Changes (Skipping Copybara)
When iterating on `Android.bp` generation without changing which files Copybara imports:
1. Run `run_gn2bp.py` with `--skip-copybara` and `--keep-temporary-files`:
   ```bash
   vpython3 components/cronet/gn2bp/run_gn2bp.py \
     --channel=tot \
     --skip-release-validation \
     --skip-copybara \
     --keep-temporary-files
   ```
   *(Use `python3` instead of `vpython3` if `depot_tools` is not in `PATH`.)*
2. **Ultra-fast re-runs (`gen_android_bp.py` only)**: If you only modified `gn2bp` Python code (no `BUILD.gn` changes), re-run the `gen_android_bp.py` command printed at the end of `run_gn2bp.py`'s output. This reuses the existing `--desc` JSON files and `--build_script_output` directory and finishes in a few seconds instead of re-running `gn desc` across 5 architectures.
3. **Diffing generated `out/gn2bp` files**: Save a copy of `out/gn2bp` before your change (e.g., in a scratch directory) and run `diff -ruN <baseline_out_gn2bp> out/gn2bp` to inspect every change to generated `Android.bp` files and `copybara_generated_paths.bara.sky`.
4. **Copying generated files into `<ANDROID_REPO>` for quick compilation**:
   ```bash
   (cd out/gn2bp && find -type f | while read -r line; do
     dest="<ANDROID_REPO>/external/cronet/tot/$line"
     if [[ -d "${dest%/*}" ]]; then cp "$line" "$dest"; fi
   done)
   ```

#### 4. Full Local Copybara Import Verification
Whenever your change modifies `copy.bara.sky`, `copybara_generated_paths.bara.sky`, source/header lists, or added/deleted files, you **must** verify a full Copybara import into `<ANDROID_REPO>/external/cronet`:
1. Commit your change on a local branch in `<CHROMIUM_REPO>` (Copybara reads from the git commit in `<CHROMIUM_REPO>`).
2. Run `run_gn2bp.py` with `--git-url-and-branch`:
   ```bash
   vpython3 components/cronet/gn2bp/run_gn2bp.py \
     --channel=tot \
     --skip-release-validation \
     --git-url-and-branch file://<ANDROID_REPO>/external/cronet <branch_name>
   ```
3. In `<ANDROID_REPO>/external/cronet`, check out `<branch_name>`:
   ```bash
   git -C <ANDROID_REPO>/external/cronet checkout <branch_name>
   ```

#### 5. Compiling and Verifying in `<ANDROID_REPO>`
**Important**: Because compiling Android is a resource-intensive process, always confirm with the user before starting an Android build if they have not already asked you to run builds in `<ANDROID_REPO>`.
Run the build in `<ANDROID_REPO>` targeting both production and Cronet test targets.
Also run the `gn2bp` unit tests in Chromium.

---

### Workflow B: Iterating Over Chromium's CQ (When Local Checkouts Are Unavailable)

If the user cannot provide access to local Chromium and Android checkouts:

1. **Local Generation & Unit Tests (No Copybara)**:
   * You can still run `gn2bp` in `--skip-copybara` mode inside the Chromium workspace to inspect `out/gn2bp/` and verify unit tests:
     ```bash
     python3 components/cronet/gn2bp/run_gn2bp.py \
       --channel=tot \
       --skip-release-validation \
       --skip-copybara \
       --keep-temporary-files
     ```
2. **Triggering the Platform Cronet Trybot on Chromium CQ**:
   * The `cronet-arm64-gn2bp-debug` internal trybot takes a Chromium CL, runs the full `gn2bp` + Copybara import pipeline, creates the corresponding Android Gerrit CL, and compiles it against Android.
   * Add the following footer to the Chromium CL description:
     ```
     Cq-Include-Trybots: luci.chrome.try:cronet-arm64-gn2bp-debug
     ```
   * Ask the user to upload the CL (`git cl upload`) and trigger the `cronet-arm64-gn2bp-debug` trybot (or trigger it if authorized).
3. **Finding the Generated Android CL from the Trybot Run**:
   * On the Chromium CL in Gerrit, open the **Checks** tab and click on `cronet-arm64-gn2bp-debug`.
   * Under the **`compile (with patch)`** step, open **`stdout`**.
   * Near the bottom of the `stdout` log, you will find the link to the generated Android Gerrit CL.
   * **Note**: The generated Android CL includes *all* Chromium commits since the last official import (not just your CL). If `main` is currently broken for Platform Cronet, the trybot will reflect that breakage too—check `components/cronet/gn2bp/breakages.json` and [go/cronet-platform-cls](http://go/cronet-platform-cls).

---

### Debugging GN2BP
If the generated `Android.bp` modules don't look correct or if a target is missing, debug the translation pipeline by keeping the intermediate files with `--keep-temporary-files`:
```bash
python3 components/cronet/gn2bp/run_gn2bp.py --skip-copybara --skip-release-validation --channel tot --keep-temporary-files
```
This preserves the temporary `gn desc ... --format=json` files for each architecture. You can inspect these raw description files to see what GN reported for a specific target (`cflags`, `cflags_cc`, `deps`, `sources`, `inputs`, `outputs`, `args`, etc.) and trace how `gn_utils.py`, `translators.py`, `action_sanitizers.py`, and `gen_android_bp.py` map those properties to Soong attributes.

### Modifying GN2BP Behavior
1. **Adding a new GN Action**: If Chromium introduces a new `action` script, add a corresponding `BaseActionSanitizer` subclass in `action_sanitizers.py` (and register it in `get_action_sanitizer()`) to translate its arguments to a Soong `genrule`.
2. **Adding Dependencies**: Exact file paths (`GENERATED_ORIGIN_FILES`) and required globs (`GENERATED_ORIGIN_GLOBS`) for Copybara are extracted automatically from the GN graph, generated Soong blueprints, and `README.chromium` metadata into `out/gn2bp/copybara_generated_paths.bara.sky` by `gen_android_bp.py`. If Cronet adds a new dependency that is replaced by an Android platform library or needs files excluded from Copybara, update `builtin_deps` / `replace_deps` in `translation_config.py` or `COPYBARA_EXCLUDED_PREFIXES` in `gen_android_bp.py`.
3. **Rust Build Scripts**: If a Rust crate relies on `OUT_DIR` and `build.rs`, ensure `generate_build_scripts_output.py` captures the generated code and that the output paths are referenced properly.

---

## Current Shortcomings of `gn2bp` to Tackle

The following architectural limitations and remaining hardcoded workarounds in `gn2bp` should be eliminated over time by deriving information directly from GN:

1. **Hardcoded Test APK Packaging in `create_test_From_apk()` (`gen_android_bp.py`)**:
   * `create_test_From_apk()` currently hardcodes test `AndroidManifest.xml` paths (`components/cronet/android/test/javatests/AndroidManifest.xml`, `testing/android/native_test/java/AndroidManifest.xml`), `resource_dirs`, `asset_dirs`, `jni_libs`, `instrumentation_target_package`, and test data paths instead of extracting them from the corresponding GN test APK targets (`//components/cronet/android:cronet_test_instrumentation_apk`, `//components/cronet/android:cronet_unittests_android`, `//net:net_unittests`).
2. **Remaining Hardcoded Per-Target Overrides in `create_additional_args()` (`context.py`)**:
   * `context.py` still contains manual per-target overrides in `create_additional_args()`, such as:
     * `//third_party/googletest:gtest` `extra_include_dir_deps`: Needed because `gtest_all.cc` textually `#include`s `.cc` files (like `src/gtest-port.cc`) that live only in `//third_party/googletest:gtest`'s `sources`, so dependent targets do not automatically receive `third_party/googletest/src/googletest` in their `include_dirs`.
     * Special-case target modifications for `//third_party/boringssl`, `//base:build_date`, and host/device target adjustments.
3. **Remaining Hardcoded Modules in `create_extras_modules()` (`gen_android_bp.py`) & Pending Two-Phase AOSP Cleanups**:
   * `create_extras_modules()` and `templates/Android.extras.bp.template` still define manual Soong modules (`license`, `jni_headers`, `cronet_java_tester_app`, `net_java_tester_app`, `android_test_helper_app`, etc.).
   * **Pending two-phase cleanups**:
     * Remove the `TODO` compatibility wrapper modules (`cronet_aml_components_cronet_android_cronet_api_java`, `cronet_aml_components_cronet_android_cronet_javatests`, `cronet_aml_testing_android_native_test_native_test_java`) from `create_extras_modules()` once the Chromium changes land in `platform/external/cronet`.
     * Delete the stub `components/cronet/android/test/src/org/chromium/net/AndroidManifest.xml` in `platform/external/cronet` (and remove its `copy.bara.sky` comment) once `${GN2BP_MODULE_PREFIX}java_tests` as a `java_library` has rolled into both `tot` and `stable`.
4. **Special-Cased `jni_zero` Logic (`action_sanitizers.py` / `translators.py`) & Global Warning Suppressions (`create_cc_defaults_module()` in `gen_android_bp.py`)**:
   * `JniZeroActionSanitizer` (`action_sanitizers.py`) and `translators.py` rely on custom heuristics to separate generated `jni_zero` placeholder classes from proxy classes so placeholder classes do not collide on the final Java classpath.
   * `create_cc_defaults_module()` in `gen_android_bp.py` still injects global `-Wno-*` flags (`-Wno-error=return-type`, `-Wno-missing-field-initializers`, `-Wno-sign-compare`, `-Wno-ambiguous-reversed-operator`, etc.) to override Soong's global `-Werror` defaults and Clang version skew between Chromium and Android. Ideally, warning suppressions should be scoped or derived from Chromium's warning mappings (`--warning-suppression-mappings`) rather than maintained globally in `cc_defaults`.
5. **Rust `build.rs` Output Handling (`b/447592983`) & GN `public` Header Visibility (`gn_utils.py`)**:
   * `translators.py` / `gen_android_bp.py` currently hardcode the `arm64` subdirectory (`gn2bp_rust_build_script_outputs/arm64/...`) when referencing pre-generated `build.rs` outputs (`b/447592983` / `b/447593242`), assuming `build.rs` outputs do not vary across architectures.
   * `gn_utils.py` treats all `.h` files in a target's `sources` as exported headers even when GN's `public` attribute is specified, which can leak internal headers across targets in Soong.
6. **Heuristic GN Action and APK Unwrapping in `gn_utils.py`**:
   * `gn_utils.py` currently inspects script names (e.g. `generate_android_wrapper.py`, `write_build_config.py`) and hardcodes internal template suffixes (e.g. `__build_config_crbug_908819`, `__write_deps`) to unwrap Java libraries, shared native libraries, and test manifests from complex GN targets.
   * This reverse-engineering of template internals could be simplified by defining explicit GN `metadata` keys on the upstream templates to expose packaged test shared libraries, Java dependencies, and manifests directly.
7. **`libcxx_headers` Glob Workarounds & Incomplete `exclude_srcs` Handling (`gen_android_bp.py` / `soong_ast.py`)**:
   * **Invariants to preserve when fixing forward**:
     * **CC action sandboxing (`b/532185687`)**: Default `CCIncludeDirPhony` in Soong on `include_dirs` only captures files with header extensions (`.h`, etc.), so extensionless `libc++` headers (`<vector>`, `<algorithm>`, `__config`, `__assertion_handler`, `__config_site`) and `.ipp` files must remain exposed in `${GN2BP_MODULE_PREFIX}libcxx_headers` (wired via `extra_include_dir_deps` on `${GN2BP_MODULE_PREFIX}cc_defaults`).
     * **`build.ninja` size (`b/578125357`, `crrev.com/c/8663081`)**: Do **not** revert `libcxx_headers` in `Android.bp` to an unexcluded recursive `third_party/libc++/src/include/**/*` glob, which expands from ~129 files to ~1,711 files (including ~1,475 `.h` files already tracked by `CCIncludeDirPhony` and ~396 frozen C++03 headers under `__cxx03/`) and bloats `build.ninja`.
   * **Trailing `/* -> /**` rewrite in `extract_copybara_paths()` (`_add_path()`)**:
     * *Problem*: `_add_path()` currently rewrites all trailing `/*` globs to `/**` (`elif clean_path.endswith("/*"): clean_path = f"{clean_path}*"`). This was added so the shallow `third_party/libc++/src/include/*` glob on `libcxx_headers` still emits `third_party/libc++/src/include/**` into `GENERATED_ORIGIN_GLOBS`, importing nested `libc++` sub-headers (`__*/*.h`) even if a top-level stdlib header is only `#include`d by Soong build-time generated C++ (`protoc`, `crubit`, `cxxbridge`). However, this globally corrupts shallow `/*` globs into recursive `/**` globs and bypasses file-exact Copybara imports (`b/576092294`) for `third_party/libc++/src/include/**` (importing dead `__cxx03/**`, `CMakeLists.txt`, `*.in`, `*.modulemap*`).
     * *Fix-forward*: Do **not** simply delete the `elif clean_path.endswith("/*"):` branch by itself—`cpp_queue` only seeds paths ending in `_CPP_SOURCE_EXTENSIONS` (skipping extensionless headers), which would break Soong-generated C++ includes and `test_extract_copybara_paths`. Instead:
       1. Remove `elif clean_path.endswith("/*"): clean_path = f"{clean_path}*"` from `_add_path()`.
       2. Because `write_copybara_paths_file()` has no exclude list, in `extract_copybara_paths()`, whenever a module has **non-empty `exclude_srcs`** (leave modules with empty `exclude_srcs` like `net_unittests_data` and `javatests_resources` untouched so their `/**` globs remain in `GENERATED_ORIGIN_GLOBS`), expand its `srcs` globs against `REPOSITORY_ROOT` minus `exclude_srcs` into exact file paths (passing literal non-glob `srcs` not matching `exclude_srcs` to `_add_path()`), and **seed those resolved files into `cpp_queue`** alongside `_CPP_SOURCE_EXTENSIONS` so `_CPP_INCLUDE_REGEX` transitively discovers all nested `third_party/libc++/src/include/__*/*.h` headers into `GENERATED_ORIGIN_FILES`. (Also exclude `third_party/libc++/src/include/__cxx03/` from `#include` traversal, since top-level `libc++` headers contain `#if ... _LIBCPP_USE_FROZEN_CXX03_HEADERS` `#include <__cxx03/...>` blocks that regex scanning would otherwise follow.)
       3. Update `test_extract_copybara_paths` in `tests/run_gn2bp_unittest.py` to create dummy files under `tmp_dir / "third_party/libc++/src/include/"` (e.g., extensionless `vector` containing `#include <__vector/vector.h>`, `__vector/vector.h`, and an excluded `CMakeLists.txt`) and assert `vector` and `__vector/vector.h` are in `generated_paths` while `CMakeLists.txt` and `third_party/libc++/src/include/**` are not.
   * **Missing `exclude_srcs` rebasing in `_rebase_module()` (`gen_android_bp.py`)**:
     * *Problem*: `FilegroupModule` (`soong_ast.py`) supports `exclude_srcs`, which only works today because `${GN2BP_MODULE_PREFIX}libcxx_headers` stays in the root `Android.bp` (`blueprint_path == ""`). `_rebase_module()` rebases `srcs` via `_rebase_files()`, but ignores `exclude_srcs`, so any `FilegroupModule` rebased into a subpackage `Android.bp` would pair rebased `srcs` with un-rebased repo-root-relative `exclude_srcs`.
     * *Fix-forward*: Rebase `module_copy.exclude_srcs` with `_rebase_files(..., blueprint_path)` in `_rebase_module()` (returning `None` on failure, matching `module_copy.srcs`) and add a unit test in `tests/run_gn2bp_unittest.py`.
   * **Nested extensionless `libc++` headers & silent target lookup in `create_extras_modules()` (`gen_android_bp.py`)**:
     * *Problem 1*: `srcs: ["third_party/libc++/src/include/*"]` + `exclude_srcs: [".../*.*"]` only covers **top-level** extensionless headers, while `CCIncludeDirPhony` in Soong only covers nested headers **with** extensions—leaving nested extensionless headers (currently `experimental/iterator`, `experimental/memory`, `experimental/propagate_const`, `experimental/simd`, `experimental/type_traits`, `experimental/utility`, `ext/__hash`, `ext/hash_map`, and `ext/hash_set`) uncovered for CC action sandboxing.
     * *Fix-forward 1*: If Cronet or a future `libc++` roll includes nested extensionless headers, switch `libcxx_headers` to `srcs: ["third_party/libc++/src/include/**/*"]` with `exclude_srcs: ["third_party/libc++/src/include/**/*.*", "third_party/libc++/src/include/__cxx03/**/*"]` (`__cxx03/**/*` **must** be excluded so its ~90 extensionless C++03 headers do not re-bloat `build.ninja`; this keeps the filegroup at ~138 files).
     * *Problem 2 & Fix-forward 2*: `buildtools/third_party/libc++/BUILD.gn` has `TODO(crbug.com/40273848)` to move to `third_party/libc++/BUILD.gn`. Support `//third_party/libc++:libc++` as a fallback or fail fast instead of silently skipping `if libcxx_target_name in gn.all_targets:` when `libc++` is expected in the graph.

# GN2BP Agent Documentation

## Overview

`gn2bp` is the toolchain used to bridge Chromium's GN build system with Android's Soong build system (`Android.bp`). Its primary goal is to take a set of target GN labels (such as Cronet), parse their descriptions, and translate them into Android.bp modules. Finally, it uses Copybara to automatically import the Chromium code and the generated `Android.bp` files into the Android repository.

This tool is especially tailored for Cronet's import into Android, meaning it contains many Cronet-specific and Android-specific workarounds.

## High-Level Workflow

The entry point for the process is typically `run_gn2bp.py`. The overall pipeline looks like this:

1. **Release Validation**: `run_gn2bp.py` fetches known breakages from `breakages.json` and verifies that the current Git history doesn't contain a bad commit without its corresponding fix.
2. **GN Config and Build Scripts Execution**: `generate_build_scripts_output.py` is invoked to compile and run any Rust build scripts (`build.rs`). Soong does not support executing Rust build scripts dynamically at build time, so Chromium pre-evaluates them and saves the resulting `cargo_flags.rs` and other generated Rust code.
3. **GN Desc Evaluation**: `gn desc ... --format=json` is run for each architecture (`x86`, `x64`, `arm`, `arm64`, `riscv64`) to extract the full GN dependency graph and target properties.
4. **Target Translation**: `gen_android_bp.py` processes the GN descriptions using `gn_utils.py`, creates an in-memory `Blueprint` and `Module` representation of each target, applies customizations, and writes the output to `Android.bp.gn2bp` files.
5. **Additional Files**: Generates licenses, Boringssl rules, and tests configuration (`AndroidTest.xml`).
6. **Copybara Import**: `run_gn2bp.py` invokes Copybara with `copy.bara.sky`. Copybara transforms the checkout (e.g., removing unneeded files, renaming `Android.bp.gn2bp` to `Android.bp`), packages the needed files, and uploads them as a Gerrit changelist (CL) to the Android's `external/cronet` project.

## Core Components

### `run_gn2bp.py`
The driver script. It handles concurrency for GN desc generation across different architectures, invokes `gen_android_bp.py`, generates `Android.extras.bp`, and orchestrates the Copybara import. It supports importing into different channels: `tot` (Tip of Tree) and `stable`.

### `gn_utils.py`
The GN JSON parser.
* Parses the output of `gn desc` and translates GN targets into `GnParser.Target` objects.
* **`source_set` Bubbling**: Soong doesn't have a direct equivalent to GN's `source_set`. `gn_utils.py` fakes it by bubbling up the dependencies, `cflags`, `defines`, and `include_dirs` of `source_set` targets to their dependent linker units (`static_library`, `shared_library`, `executable`).
* Consolidates architecture-specific arguments, keeping common flags in a `common` arch and pushing architecture-specific flags to their respective arch object.

### `gen_android_bp.py`
The core GN to Soong translator.
* Maps GN targets to `Module` objects (which represent Soong modules like `cc_library_static`, `cc_genrule`, `java_library`, `rust_ffi_static`, etc.).
* Implements `builtin_deps` and `replace_deps` to swap out Chromium dependencies for their Android equivalents (e.g., routing `//third_party/zlib:zlib` to Android's `libz`, swapping `androidx` dependencies).
* Splits the overall blueprint into smaller `Android.bp` files per directory to comply with Android rules (e.g., `third_party/rust/...` crates get their own `Android.bp`).

### Action Sanitizers (in `gen_android_bp.py`)
GN `action` targets execute arbitrary Python scripts. Soong's `genrule` executes shell commands. `gen_android_bp.py` uses `BaseActionSanitizer` subclasses to translate GN script invocations into Soong-compatible bash commands.
* **JNI Generators**: Translates JNI generation actions into Soong rules. Handles creating proxy classes and placeholder classes, sometimes separating them so placeholder classes don't pollute the classpath.
* **Protos**: GN proto targets are split into two `cc_genrule`s: one for `.h` generation and one for `.cc` generation.
* **CXX & Bindgen**: Translates Rust-C++ interop actions into the appropriate `rust_bindgen` or `cc_genrule` modules.

### `copy.bara.sky`
The Copybara configuration file. Defines `origin_files` (what to import from Chromium) and performs file renames (`*.gn2bp` -> `*`). Defines workflows for both `tot` and `stable` branches. Excludes test files and non-essential third-party directories to keep the Android checkout slim.

## Language Specific Handling

### Java
Translating GN's `java_library` to Soong requires simulating GN's jar filtering (`jar_included_patterns`/`jar_excluded_patterns`). GN2BP achieves this by creating three Soong modules for a single GN target:
1. `__unfiltered`: The actual `java_library` that compiles the code.
2. `__filtered`: A `java_genrule` that uses `filter_zip.py` to strip out excluded classes.
3. The top-level module: A `java_library` acting as a dependency holder that statically links the filtered jar.

### Rust
* **Bindgen**: Mapped to `rust_bindgen`.
* **Crates**: Mapped to `rust_ffi_static` or `rust_binary`. The `crate_name` is extracted, and target names are hashed to avoid file path length limits in Soong.
* **Build Scripts**: Because Soong doesn't run `build.rs` scripts dynamically, `generate_build_scripts_output.py` pre-evaluates them and saves outputs in `gn2bp_rust_build_script_outputs/`.

### C++
* Extracts `cflags`, `ldflags`, `include_dirs`, and links them to the proper targets.
* Removes flags that are forbidden or unnecessary in Soong (e.g., `-gsplit-dwarf`, `-Wl,--gc-sections`).

## Important Concepts & Gotchas

* **FIPS Compliance / Libcrypto Stripping**: Cronet links against Boringssl. For FIPS compliance in Android, `libcrypto` must be a shared library (`libcrypto_httpengine.so`). Because this prevents dead-code elimination, GN2BP uses a two-step linker script generation (`generate_libcronet_libcrypto_symbol_linker_script` in `Android.extras.bp.template`) to extract only the symbols Cronet actually uses, pruning the final `libcrypto` output.
* **`Android.extras.bp`**: Used for manually maintained Soong modules that cannot be easily autogenerated from GN (e.g., high-level `java_library` defaults, `android_test` configurations, `cc_library_headers` workarounds).
* **`MODULE_PREFIX`**: Most generated module names are prefixed with a channel-specific prefix (e.g., `tot_cronet_` or `stable_cronet_`) to avoid collisions between the Tip-of-Tree and Stable channel imports inside the Android tree.
* **Rebasing Blueprints**: `gen_android_bp.py` breaks down the single massive Blueprint into multiple smaller files and places them near their source files, rebasing file paths accordingly.

## How to use as an Agent

### Internal Documentation
If the user is a Googler or has access to Google-internal resources, it is highly recommended to read [go/cronet-platform](http://go/cronet-platform) (or use the `retrieve_content` tool to look it up) for essential context on the Cronet Android platform architecture, procedures, and related systems.

### Running GN2BP
You can run GN2BP locally to generate the build files and see your changes. It's recommended to run it with `--skip-copybara` so it only generates the `Android.bp.gn2bp` files without actually trying to import them into an Android Gerrit CL.
Since you are likely not running this in a CI environment, you must explicitly provide the `--channel` argument (e.g. `tot` or `stable`) and should use `--skip-release-validation` to avoid git history checks failing in a shallow clone. Also, use `python3` instead of `vpython3` if depot_tools are not available.
```bash
python3 components/cronet/gn2bp/run_gn2bp.py --skip-copybara --skip-release-validation --channel tot
```

### Iterating Quickly on Android
If you need to quickly test the generated `Android.bp` files in an actual Android build environment without waiting for a full Copybara run:

**Important**: Because compiling Android is a time-consuming process, **you must first ask the user** if they wish to proceed with this option. Do not do it automatically.

If the user agrees:
1. Ask the user for the absolute path to their Android checkout (specifically the `external/cronet` directory on their local file system).
2. Manually copy the generated files over to the Android checkout. For the `tot` channel, this looks like:
   ```bash
   # Assuming GN2BP was just run with --skip-copybara
   android_cronet_dir="/path/to/your/android/checkout/external/cronet/tot"
   find -name Android.bp.gn2bp | while read -r line; do cp "$line" "$android_cronet_dir/${line%.gn2bp}"; done
   cp third_party/boringssl/sources.bp "$android_cronet_dir/third_party/boringssl/sources.bp"
   cp third_party/boringssl/sources.mk "$android_cronet_dir/third_party/boringssl/sources.mk"
   cp Android.extras.bp.gn2bp "$android_cronet_dir/Android.extras.bp"
   ```
3. Change to the root of the Android checkout, set up the Android build environment using `source` and `lunch`, and compile the necessary targets. **Note:** This should be done in a different terminal window so it does not poison the user's current terminal environment:
   ```bash
   cd /path/to/your/android/checkout
   source build/make/rbesetup.sh
   lunch aosp_cf_x86_64_phone-trunk_staging-eng
   m com.android.tethering
   ```

### Debugging GN2BP
If the generated Android.bp modules don't look correct or if a target is missing, you can debug the translation pipeline by keeping the intermediate files.
Run `run_gn2bp.py` with the `--keep-temporary-files` flag:
```bash
python3 components/cronet/gn2bp/run_gn2bp.py --skip-copybara --skip-release-validation --channel tot --keep-temporary-files
```
This will preserve the temporary directories containing the raw GN description JSON files (`gn desc ... --format=json`). You can inspect these raw description files to see exactly what GN reported for a specific target (e.g., its `cflags`, `deps`, `sources`, or `args`) and connect it with how `gen_android_bp.py` maps these properties to Soong attributes.

### Modifying GN2BP behavior
1. **Adding a new GN Action**: If Chromium introduces a new `action` script, you must add a corresponding `ActionSanitizer` in `gen_android_bp.py` to translate its arguments to a Soong `genrule`.
2. **Adding Dependencies**: If Cronet adds a new third-party dependency, you may need to add it to the `origin_files` in `copy.bara.sky` or map it to an existing Android library in `builtin_deps` in `gen_android_bp.py`.
3. **Rust Build Scripts**: If a Rust crate heavily relies on `OUT_DIR` and `build.rs`, ensure `generate_build_scripts_output.py` successfully captures the generated code and that the output paths are correctly referenced.

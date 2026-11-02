---
name: webapps-dev
description: >-
  How to develop, build, and test WebApps / PWA features across Desktop and
  Android. This includes installation & related features, TWAs, WebAPKs,
  etc.
---

# WebApps Development

Supplementary engineering information for the WebApps/PWA product area.

[Central Hub & Directory Map](/components/webapps/AGENTS.md)

## Development Procedures & Gotchas

1. **Format CL descriptions**: Write CL descriptions with the `cl-description`
   skill. Wrap lines with
   `agents/shared/skills/cl-description/scripts/wrap_lines.py`. Never wrap lines
   by hand, and keep the `Change-Id:` footer.
2. **Pass commit messages by file**: Run `git commit -F <file>`. Pass `-n +` to
   `git cl description` to load `HEAD`, or `-n -` to read stdin. Never pass a
   file path to `git commit -m`, `git cl upload --commit-description`, or
   `git cl description -n` (`--new-description`). Those flags use the input
   string literally.
3. **Preserve revert quotes**: On revert or reland CLs, put extra-change notes
   above `Original change's description:`. Keep `> ` quoted lines unwrapped, and
   do not pass them to `wrap_lines.py`.
4. **Format stacked CL subjects**: On stacked CLs, format the subject as
   `[<tag>] <Title> [k/N]`, using 1-based `k` and the literal letter `N`. Use
   `[webapps]` by default, or a short chain tag that groups related CLs in
   Gerrit search (for example, `[RefactorFoo]`).
5. **Track parent branches**: When creating a stacked branch, run
   `git branch --set-upstream-to=<parent-branch>` immediately so `git cl upload`
   chains the CL.
6. **Sequence changes for reviewability**: Separate mechanical changes (moves,
   renames, formatting, generated code) from behavioral changes, usually as a
   parent CL and a child CL. Mechanical CLs are fast to review on their own.
7. **Do not fetch unprompted**: Unless the user asks, do not fetch or rebase
   onto a newer `origin/main`. A newer base needs a `gclient sync`, which is
   slow. The user syncs on their own schedule.
8. **Sync DEPS once on rebase**: After rebasing the base branch onto
   `origin/main`, run `gclient sync -D` once. Child-branch rebases do not change
   `DEPS`, so do not run `gclient sync -D` before them.

## Testing Procedures & Gotchas

1. **Select a build directory**: When choosing a build directory, list
   `out/*/args.gn` and run `gn args <dir> --list --short --overrides-only` on
   each directory. Check `target_os`, `target_cpu`, and `is_desktop_android` to
   match your target platform (Linux, Windows, Android phone, desktop Android,
   etc.). If zero or multiple directories match, ask the user instead of
   guessing or creating a new directory.
2. **Use `tools/autotest.py`**: Run C++ and Java tests with
   `tools/autotest.py -C out/<Dir> --run-all <test-files>`. The `--run-all` flag
   runs every matched target and skips the target prompt. Unless `autotest.py`
   cannot target the suite, do not run `out/*/bin/run_*` wrappers directly.
3. If your command tool uses a restricted sandbox, run `tools/autotest.py`,
   `autoninja`, `siso`, `gclient`, `git cl`, `vpython3`, and `adb` unsandboxed.
   Restricted sandboxes block `vpython` caches, network access, and device
   sockets.

For cross-platform testing, a good method is to use the `utr` skill, which will
facilitate compiling for the given platform and sending the binary to an
applicable bot.

- Most platforms can cross-compile locally, so first ensure that the platform is
  in the .gclient file.
- Often the local compilation config won't match the utr config 'exactly' (e.g.
  code coverage config, etc). This is usually fine, and you can tell the script
  to continue.

### Android

1. If you run `run_chrome_public_test_apk` directly, build the target first.
2. Instrumentation tests (such as `chrome_public_test_apk`) require a connected
   device or emulator. Run `adb devices` first, and reuse any connected device
   or running emulator. If none is connected, start the AVD config from
   `tools/android/avd/proto/` that the user named. If the user did not name a
   config, ask the user and do not guess.
   - **If you only need to run tests**: Start a headless emulator by omitting
     `--emulator-window`:
     ```bash
     vpython3 tools/android/avd/avd.py start -v \
       --avd-config <path-to-config.textpb> \
       --no-read-only --enable-network
     ```
   - **If the user will inspect the build**: Reuse a visible emulator, or start
     one with `--emulator-window` and tell them:
     ```bash
     vpython3 tools/android/avd/avd.py start -v \
       --avd-config <path-to-config.textpb> \
       --emulator-window --no-read-only --enable-network
     ```
3. Unless WebAPK and TWA integration tests share a parameterized runner, put
   them in separate test files.
4. Read the
   [Android Testing Guide](/components/webapps/docs/android_testing_guide.md)
   for complete PWA and Android testing details.

## Design & Review Workflows

- For non-trivial designs or execution plans, use `harness-doc-writer` and
  coordinate reviews with `chromium_design_reviewer`.
- For security code reviews, vulnerability fix verification, or variation
  audits, use the `security-review` skill
  (`internal/agents/skills/security-review/`).
- After structural changes or new directories, audit links and maps with
  `harness-updater`.

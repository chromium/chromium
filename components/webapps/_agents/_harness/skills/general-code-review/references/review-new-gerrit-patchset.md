# Review New Gerrit Patchset Reference

1. **Find the patchsets**: `HEAD` holds the patchset that `git cl patch` applied
   (`<OLD_PS>`):

   ```bash
   git config branch.$(git branch --show-current).gerritissue
   git config branch.$(git branch --show-current).gerritpatchset
   ```

   Query Gerrit for the latest patchset `<NEW_PS>` with the
   [`gerrit-cli` skill](/agents/shared/skills/gerrit-cli/SKILL.md) (`changes`
   with `-o CURRENT_REVISION --json_file`) or Gerrit MCP tools. Do not use
   `git cl status --field=patch`, which writes to `.git/config`.

2. **Apply `<NEW_PS>`**: Delete any untracked (`??`) files left by an earlier
   patchset update or prototype, then run from `src/`:

   ```bash
   git diff --binary HEAD~1 | git apply -R
   vpython3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
     patch --host https://chromium-review.googlesource.com \
     --change <CL_NUMBER> --revision <NEW_PS> | git apply
   ```

   If `git apply` fails because `<NEW_PS>` was rebased onto conflicting upstream
   changes, the working tree is now at the base commit. Restore `<OLD_PS>` with
   `git diff --binary HEAD~1 HEAD | git apply`, report the conflict, and ask the
   caller before rebasing the checkout.

3. **Inspect**: `HEAD` is still `<OLD_PS>` and the working tree is `<NEW_PS>`.

   - `git diff` shows the `<OLD_PS>` -> `<NEW_PS>` delta (always relative to the
     committed patchset, not an earlier uncommitted one).
   - `git diff HEAD~1` shows the full `<NEW_PS>` diff.
   - `git status --short` lists new files (`??`), which neither diff includes.
     Read those directly. For large patchsets, start with `--name-only`.

---
name: components/autofill/core/browser/payments/setup-flag-on-and-off-tests-for-easy-cleanup
description: When writing flag-on and flag-off unit tests for a launch flag, give the permanent flag-on test the unsuffixed name and isolate the temporary flag-off test under a `_FlagOff` suffix for easy future cleanup
rubric: testing
severity: nit
---

**Rule.** When writing unit tests covering both enabled and disabled states of a
launch feature flag, name the permanent (flag-on) test without a flag suffix
(for example, `ShowsWalletPrompt` in C++ or `testShowsWalletPrompt` in Java) and
isolate the temporary flag-off behavior in a separate test with a `_FlagOff` or
`_FlagDisabled` suffix (for example, `ShowsWalletPrompt_FlagOff` or
`testShowsWalletPrompt_flagOff`). Avoid suffixing the flag-on test with
`_FlagOn` / `_FlagEnabled` or interleaving `if (flag_enabled)` branches inside a
single shared test body when separate tests suffice.

This applies to standard launch flags where the enabled state is the long-term
target behavior; it does not apply to permanent flags, kill-switch or
deprecation flags where the disabled state is the long-term target, or suites
already parameterized over multiple test dimensions.

**Why.** Once a launch flag is fully ramped and ready for cleanup, the flag-off
test is deleted while the flag-on test becomes the permanent test. Keeping the
unsuffixed name on the flag-on test and isolating flag-off assertions in its own
`_FlagOff` test lets flag cleanup simply delete the `_FlagOff` test and remove
the `ScopedFeatureList` / `@EnableFeatures` setup without renaming tests or
rewriting assertions inside the surviving test.

## Examples

Bad:

```cpp
// Bad: Flag-on test has a temporary suffix that will need renaming on launch,
// or both states are tangled inside one branched test.
TEST_F(PaymentsPromptTest, ShowsWalletPrompt_FlagOn) {
  base::test::ScopedFeatureList feature_list(features::kWalletPrompt);
  EXPECT_TRUE(manager_->MaybeShowPrompt());
}

TEST_F(PaymentsPromptTest, ShowsWalletPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWalletPrompt);
  EXPECT_FALSE(manager_->MaybeShowPrompt());
}
```

Good:

```cpp
// Good: Permanent flag-on test uses the clean unsuffixed name; temporary
// flag-off test is isolated under `_FlagOff` so launch cleanup only deletes
// the `_FlagOff` test and drops the feature flag override.
TEST_F(PaymentsPromptTest, ShowsWalletPrompt) {
  base::test::ScopedFeatureList feature_list(features::kWalletPrompt);
  EXPECT_TRUE(manager_->MaybeShowPrompt());
}

TEST_F(PaymentsPromptTest, ShowsWalletPrompt_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWalletPrompt);
  EXPECT_FALSE(manager_->MaybeShowPrompt());
}
```

```java
// Good (Java): Same convention with JUnit feature annotations.
@Test
@EnableFeatures(ChromeFeatureList.WALLET_PROMPT)
public void testShowsWalletPrompt() {
  assertTrue(mManager.maybeShowPrompt());
}

@Test
@DisableFeatures(ChromeFeatureList.WALLET_PROMPT)
public void testShowsWalletPrompt_flagOff() {
  assertFalse(mManager.maybeShowPrompt());
}
```

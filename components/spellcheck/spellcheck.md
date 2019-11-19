# About the 'use_browser_spellchecker' and 'use_renderer_spellchecker' build time flag.

## use_browser_spellchecker

Use the operating system's spellchecker rather than hunspell. This does
not affect the "red underline" spellchecker which can consult Google's
server-based spellcheck service.

## use_renderer_spellchecker
Use hunspell spellchecker rather than the operating system's spellchecker.

## Note:

For most operating system except Windows, the decision to use the platform
spellchecker or hunspell spellchecker is made at build time. Therefore,
use_browser_spellchecker and use_renderer_spellchecker are mutually
exclusive for most operating systems except Windows.

For Windows OS, the decision to use the platform spellchecker or hunspell
spellchecker is made during runtime. Therefore, we include both build
flags if the platform is Windows.

We also need to create the runtime feature flag kWinUseBrowserSpellChecker
for Windows OS. The feature flag is used to choose between the platform or
hunspell spellchecker at runtime.

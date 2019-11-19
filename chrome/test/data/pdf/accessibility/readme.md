# PdfExtensionAccessibilityTreeDumpTest

A `PdfExtensionAccessibilityTreeDumpTest` loads a PDF file, waits for it
to load, then dumps the accessibility tree. More about accessibility tree
dump tests at `content/test/data/accessibility/readme.md`.

## Filters and directives

Please refer to `content/test/data/accessibility/readme.md` to see
how to use filters. Supported filters and directives are:
* [x] `@<platform>-ALLOW:`
* [ ] `@<platform>-ALLOW-EMPTY:`
* [ ] `@<platform>-DENY:`
* [ ] `@WAIT-FOR:`

The PdfExtensionAccessibilityTreeDumpTest implementation of extra
directives/filters is similar to the DumpAccessibilityTreeTest. If you
need to support additional directives/filters, see
/src/content/test/data/accessibility/readme.md to respect the same
interface as the one used for HTML accessibility tree dump tests.

Implementation details for parsing extra directives in HTML can be
found in `DumpAccessibilityTestBase::ParseHtmlForExtraDirectives()`.

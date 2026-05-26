# DevTools ClientVariations Parser

This directory contains the logic for generating the `ClientVariations` protocol
buffer parser used by Chrome DevTools.

## Purpose

The `X-Client-Data` header sent in network requests to Google properties
contains serialized variations information. DevTools uses this parser to
decode and display that information in a human-readable format in the Network
panel.

## Implementation

The parser is implemented in TypeScript (`client_variations_uncompiled.ts`) and
utilizes modern TypeScript bindings generated from `client_variations.proto`.

The build process (`update_client_variations.py`) performs the following steps:
1.  Generates the TypeScript bindings for the `ClientVariations` proto.
2.  Transpiles the parser and its dependencies into JavaScript using `tsc`.
3.  Bundles the output into a single, self-contained ES module using `rollup`.

## Integration

The final bundled JavaScript file is generated at build time in the `gen/`
directory. TODO(crbug.com/40253708): Update the DevTools frontend build system
to consume this generated file.

To manually trigger a build of the parser:
```bash
autoninja -C out/Default components/variations/proto/devtools:client_variations_js
```

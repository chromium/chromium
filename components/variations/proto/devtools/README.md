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
4.  Minifies the bundled ES module using `terser`.

## Integration

The bundled and minified JavaScript file (`client_variations.js`) is checked
into this directory so that the DevTools autoroller (`scripts/deps/roll_deps.py`
in `devtools-frontend`) can sync it into the standalone DevTools repository.

It is also built at compile time in `gen/` via the
`//components/variations/proto/devtools:client_variations_js` build target.

To rebuild and update the checked-in `client_variations.js`:
```bash
python3 components/variations/proto/devtools/update_client_variations.py -t Default
```

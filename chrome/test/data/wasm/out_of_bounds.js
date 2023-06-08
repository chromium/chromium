// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains tests to exercise Wasm's in- and out- of bounds behavior.

const module_bytes = (function createModule() {
  const builder = new WasmModuleBuilder;

  builder.addImportedMemory("external", "memory");

  const peek = builder.addFunction("peek", kSig_i_i).exportFunc();
  peek.body
      .get_local(0)
      .i32_load()
      .end();

  const poke = builder.addFunction("poke", kSig_v_ii).exportFunc();
  poke.body
      .get_local(0)
      .get_local(1)
      .i32_store()
      .end();

  const grow = builder.addFunction("grow", kSig_i_i).exportFunc();
  grow.body
      .get_local(0)
      .grow_memory()
      .end();

  return builder.toBuffer();
})();

function assert_true(b) {
  if (!b) {
    throw new Error("assert_true failed");
  }
}

function assert_throws(error_class, f) {
  try {
    f()
  } catch (e) {
    if (e instanceof error_class) {
      return;
    }
  }
  throw new Error(
      "function was expected to throw " + error_class + " but did not");
}

function assert_equals(actual, expected) {
  if (actual != expected) {
    throw new Error("Expected " + expected + " but got " + actual);
  }
}

function instantiate(memory) {
  assert_true(module_bytes instanceof ArrayBuffer,
              "module bytes should be an ArrayBuffer");
  assert_true(memory instanceof WebAssembly.Memory,
              "memory must be a WebAssembly.Memory");
  return new WebAssembly.Instance(
    new WebAssembly.Module(module_bytes),  { external: { memory: memory } });
}

function instantiatePages(num_pages) {
  return instantiate(new WebAssembly.Memory({ initial: num_pages }));
}

function assert_oob(func) {
  return assert_throws(WebAssembly.RuntimeError, func);
}

function peek_in_bounds() {
  try {
    const instance = instantiatePages(1);
    const peek = instance.exports.peek;

    assert_equals(peek(0), 0);
    assert_equals(peek(10000), 0);
    assert_equals(peek(65532), 0);
  } catch(e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function peek_out_of_bounds() {
  try {
    const instance = instantiatePages(1);
    const peek = instance.exports.peek;

    assert_oob(_ => peek(65536));
    assert_oob(_ => peek(65535));
    assert_oob(_ => peek(65534));
    assert_oob(_ => peek(65533));

    assert_oob(_ => peek(1 << 30));
    assert_oob(_ => peek(3 << 30));
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function peek_out_of_bounds_grow_memory_from_zero_js() {
  const memory = new WebAssembly.Memory({initial: 0});
  try {
    const instance = instantiate(memory);
    const peek = instance.exports.peek;

    assert_oob(_ => peek(0));
    memory.grow(1);
    assert_equals(peek(0), 0);
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function peek_out_of_bounds_grow_memory_js() {
  const memory = new WebAssembly.Memory({initial: 1});
  try {
    const instance = instantiate(memory);
    const peek = instance.exports.peek;

    assert_oob(_ => peek(70000));
    memory.grow(1);
    assert_equals(peek(70000), 0);
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function peek_out_of_bounds_grow_memory_from_zero_wasm() {
  const memory = new WebAssembly.Memory({initial: 0});
  try {
    const instance = instantiate(memory);
    const peek = instance.exports.peek;
    const grow = instance.exports.grow;

    assert_oob(_ => peek(0));
    grow(1);
    assert_equals(peek(0), 0);
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function peek_out_of_bounds_grow_memory_wasm() {
  const memory = new WebAssembly.Memory({initial: 1});
  try {
    const instance = instantiate(memory);
    const peek = instance.exports.peek;
    const grow = instance.exports.grow;

    assert_oob(_ => peek(70000));
    grow(1);
    assert_equals(peek(70000), 0);
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function poke_in_bounds() {
  try {
    const instance = instantiatePages(1);
    const peek = instance.exports.peek;
    const poke = instance.exports.poke;

    poke(0, 41);
    poke(10000, 42);
    poke(65532, 43);

    assert_equals(peek(0), 41);
    assert_equals(peek(10000), 42);
    assert_equals(peek(65532), 43);
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function poke_out_of_bounds() {
  try {
    const instance = instantiatePages(1);
    const poke = instance.exports.poke;

    assert_oob(_ => poke(65536, 0));
    assert_oob(_ => poke(65535, 0));
    assert_oob(_ => poke(65534, 0));
    assert_oob(_ => poke(65533, 0));

    assert_oob(_ => poke(1 << 30, 0));
    assert_oob(_ => poke(3 << 30, 0));
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function poke_out_of_bounds_grow_memory_from_zero_js() {
  const memory = new WebAssembly.Memory({initial: 0});
  try {
    const instance = instantiate(memory);
    const peek = instance.exports.peek;
    const poke = instance.exports.poke;

    function check_poke(index, value) {
      poke(index, value);
      assert_equals(peek(index), value);
    }

    assert_oob(_ => poke(0, 42));
    memory.grow(1);
    check_poke(0, 42);
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function poke_out_of_bounds_grow_memory_js() {
  const memory = new WebAssembly.Memory({initial: 1});
  try {
    const instance = instantiate(memory);
    const peek = instance.exports.peek;
    const poke = instance.exports.poke;

    function check_poke(index, value) {
      poke(index, value);
      assert_equals(peek(index), value);
    }

    assert_oob(_ => poke(70000, 42));
    memory.grow(1);
    check_poke(70000, 42);
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function poke_out_of_bounds_grow_memory_from_zero_wasm() {
  const memory = new WebAssembly.Memory({initial: 0});
  try {
    const instance = instantiate(memory);
    const peek = instance.exports.peek;
    const poke = instance.exports.poke;
    const grow = instance.exports.grow;

    function check_poke(index, value) {
      poke(index, value);
      assert_equals(peek(index), value);
    }

    assert_oob(_ => poke(0, 42));
    grow(1);
    check_poke(0, 42);
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

function poke_out_of_bounds_grow_memory_wasm() {
  const memory = new WebAssembly.Memory({initial: 1});
  try {
    const instance = instantiate(memory);
    const peek = instance.exports.peek;
    const poke = instance.exports.poke;
    const grow = instance.exports.grow;

    function check_poke(index, value) {
      poke(index, value);
      assert_equals(peek(index), value);
    }

    assert_oob(_ => poke(70000, 42));
    grow(1);
    check_poke(70000, 42);
  } catch (e) {
    console.error("uncaught exception: " + e);
    return false;
  }
  return true;
}

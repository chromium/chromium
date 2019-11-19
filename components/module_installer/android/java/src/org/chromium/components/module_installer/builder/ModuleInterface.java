// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Denotes an interface to be the interface of a feature module. For a module with name foo, this
 * annotation will generate a class FooModule that offers the same functionality as {@link Module}.
 */
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.SOURCE)
public @interface ModuleInterface {
    /** The name of the module. */
    String module();
    /** The fully qualified name of the module's interface implementation. */
    String impl();
}

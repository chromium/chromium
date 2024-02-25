// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_DATABINDING_BINDING_H_
#define CHROME_BROWSER_VR_DATABINDING_BINDING_H_

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/vr/databinding/binding_base.h"

namespace vr {

// This class represents a one-way binding that propagates a change from a
// source/model property to a sink/view. This is inappropriate for use in the
// case of, say, an editor view like a text field where changes must also be
// propagated back to the model.
//
// IMPORTANT: it is assumed that a Binding instance will outlive the model to
// which it is bound. This class in not appropriate for use with models that
// come and go in the lifetime of the application.
template <typename T>
class Binding : public BindingBase {
 public:
#ifndef NDEBUG
  Binding(const base::RepeatingCallback<T()>& getter,
          const std::string& getter_text,
          const base::RepeatingCallback<void(const T&)>& setter,
          const std::string& setter_text)
      : getter_(getter),
        setter_(setter),
        getter_text_(getter_text),
        setter_text_(setter_text) {}

  Binding(const base::RepeatingCallback<T()>& getter,
          const std::string& getter_text,
          const base::RepeatingCallback<void(const std::optional<T>&,
                                             const T&)>& setter,
          const std::string& setter_text)
      : getter_(getter),
        historic_setter_(setter),
        getter_text_(getter_text),
        setter_text_(setter_text) {}
#else
  Binding(const base::RepeatingCallback<T()>& getter,
          const base::RepeatingCallback<void(const T&)>& setter)
      : getter_(getter), setter_(setter) {}

  Binding(const base::RepeatingCallback<T()>& getter,
          const base::RepeatingCallback<void(const std::optional<T>&,
                                             const T&)>& setter)
      : getter_(getter), historic_setter_(setter) {}
#endif

  Binding(const Binding&) = delete;
  Binding& operator=(const Binding&) = delete;

  ~Binding() override = default;

  // This function will check if the getter is producing a different value than
  // when it was last polled. If so, it will pass that value to the provided
  // setter. NB: this assumes that T is copyable.
  bool Update() override {
    T current_value = getter_.Run();
    if (last_value_ && current_value == last_value_.value())
      return false;
    if (setter_)
      setter_.Run(current_value);
    if (historic_setter_)
      historic_setter_.Run(last_value_, current_value);
    last_value_ = current_value;
    return true;
  }

  std::string ToString() override {
#ifndef NDEBUG
    if (getter_text_.empty() && setter_text_.empty())
      return "";

    return base::StringPrintf("%s => %s", getter_text_.c_str(),
                              setter_text_.c_str());
#else
    return "";
#endif
  }

 private:
  base::RepeatingCallback<T()> getter_;
  base::RepeatingCallback<void(const T&)> setter_;
  base::RepeatingCallback<void(const std::optional<T>&, const T&)>
      historic_setter_;
  std::optional<T> last_value_;

#ifndef NDEBUG
  std::string getter_text_;
  std::string setter_text_;
#endif
};

// These macros are sugar for constructing a simple binding. It is meant to make
// setting up bindings a little less painful, but it is not meant to handle all
// cases. If you need to do something more complex (eg, convert type T before
// it is propagated), you should use the constructor directly.
//
// For example:
//
// struct MyModel { int source; };
// struct MyView {
//   int sink;
//   int awesomeness;
//   void SetAwesomeness(int new_awesomeness) {
//     awesomeness = new_awesomeness;
//   }
// };
//
// MyModel m;
// m.source = 20;
//
// MyView v;
// v.sink = 10;
// v.awesomeness = 30;
//
// auto binding = VR_BIND(int, MyModel, &m, source, MyView, &v, sink = value);
//
// Or, equivalently:
//
// auto binding = VR_BIND_FIELD(int, MyModel, &m, source, MyView, &v, sink);
//
// If your view has a setter, you may find VR_BIND_FUNC handy:
//
// auto binding =
//     VR_BIND_FUNC(int, MyModel, &m, source, MyView, &v, SetAwesomeness);
//
#ifndef NDEBUG
#define VR_BIND(T, M, m, Get, V, v, Set)                                      \
  std::make_unique<Binding<T>>(                                               \
      base::BindRepeating([](M* model) { return Get; }, base::Unretained(m)), \
      #Get,                                                                   \
      base::BindRepeating([](V* view, T const& value) { Set; },               \
                          base::Unretained(v)),                               \
      #Set)
#else
#define VR_BIND(T, M, m, Get, V, v, Set)                                      \
  std::make_unique<Binding<T>>(                                               \
      base::BindRepeating([](M* model) { return Get; }, base::Unretained(m)), \
      base::BindRepeating([](V* view, T const& value) { Set; },               \
                          base::Unretained(v)))
#endif

#define VR_BIND_FUNC(T, M, m, Get, V, v, f) \
  VR_BIND(T, M, m, Get, V, v, view->f(value))

#define VR_BIND_FIELD(T, M, m, Get, V, v, f) \
  VR_BIND(T, M, m, Get, V, v, view->f = value)

#ifndef NDEBUG
#define VR_BIND_LAMBDA(...) base::BindRepeating(__VA_ARGS__), #__VA_ARGS__
#else
#define VR_BIND_LAMBDA(...) base::BindRepeating(__VA_ARGS__)
#endif

}  // namespace vr

#endif  // CHROME_BROWSER_VR_DATABINDING_BINDING_H_
